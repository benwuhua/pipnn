# Mutation Scoring Design — PiPNN PoC

- Date: 2026-03-12
- Status: Draft for user review
- Scope: replace the current `blocked-state` mutation workflow with a reproducible, score-producing remote x86 mutation pipeline for the project-owned `src/` increment set

## 1. Goal

Convert `NFR-006` from "probe-only / blocked-state evidence" into a real mutation score workflow that runs authoritatively on the remote x86 host.

This round is intentionally narrow:
- mutation scope is limited to project-owned source files under `src/`
- the first scored set is the increment subset:
  - `src/cli/app.cpp`
  - `src/bench/runner.cpp`
  - `src/core/hashprune.cpp`
  - `src/core/leaf_knn.cpp`
  - `src/core/pipnn_builder.cpp`
  - `src/core/rbc.cpp`
  - `src/search/greedy_beam.cpp`
- `tests/` and third-party code (`_deps`, system headers, hnswlib sources) are excluded from mutation scoring
- the remote x86 host is the only authoritative mutation environment

## 2. Success Criteria

The design is complete when all of the following are true:
- a user-space `LLVM + Mull` toolchain can be installed and reused on the remote host without modifying the system environment
- one repo-local command can run the end-to-end mutation workflow
- the workflow emits per-executable raw reports plus an aggregate mutation score
- the aggregate score for the increment set is at least `80`
- `scripts/validate_mutation_evidence.py`, `results/repro_manifest.json`, and the ST report support a scored-state path in addition to the existing blocked-state path

## 3. Constraints

- Do not modify the current normal build (`build/`) or coverage build (`build-cov/`) workflow.
- Do not require system package installation on the remote host.
- Do not require the local macOS machine to run mutation scoring.
- Do not widen scope to all `src/` files in this round.
- Do not couple the mutation pipeline to full `ctest`; use a targeted test set only.

## 4. Recommended Approach

### Option A — Recommended: dedicated remote mutation toolchain + targeted score

- Install fixed versions of `LLVM` and `Mull` into a repo-local tool directory on the remote host.
- Create a dedicated mutation build directory, `build-mull/`, configured with `clang/clang++` and `-DPIPNN_ENABLE_OPENMP=OFF`.
- Run `mull-runner` separately against a targeted set of test executables.
- Aggregate the resulting JSON reports into one score and one survivor summary.

Why this is recommended:
- isolates mutation from the regular dev and coverage builds
- avoids GCC/Clang and OpenMP cross-talk
- gives the fastest path to a stable `>= 80` result
- preserves the current `Go` baseline while the new pipeline is brought up

### Option B — full-`src` score immediately

- Same pipeline as Option A, but widen scoring to all `src/` files from the start.

Trade-off:
- better end-state coverage, but much higher risk on runtime and troubleshooting

### Option C — local + remote symmetry

- Install and run the same mutation pipeline on both local and remote environments.

Trade-off:
- improves environment symmetry, but adds cost without improving the authoritative gate in this round

## 5. Architecture

### 5.1 Toolchain Layer

Add a repo-local toolchain installer:
- `scripts/quality/ensure_remote_mull_toolchain.sh`

Responsibilities:
- install fixed-version `LLVM` and `Mull` under remote repo-local directories such as:
  - `.tools/llvm/`
  - `.tools/mull/`
- expose stable executable paths for:
  - `clang`
  - `clang++`
  - `mull-runner`
- verify version compatibility before the mutation build starts

Version pinning:
- define `LLVM_VERSION` and `MULL_VERSION` as constants in the installer script
- fail fast if the installed versions are missing or incompatible

### 5.2 Execution Layer

Add two wrappers:
- `scripts/quality/remote_mutation_run.sh`
- `scripts/quality/remote_mutation_run_inner.sh`

Outer wrapper responsibilities:
- sync repo to remote
- invoke remote toolchain installer
- run a smoke mutation job first
- run the full targeted mutation job after smoke passes
- fetch reports back into `results/st/`

Inner wrapper responsibilities:
- clean and recreate `build-mull/`
- configure with the user-space `clang/clang++`
- disable OpenMP for the mutation build
- build only the targeted test executables
- run `mull-runner` once per executable
- store raw reports and aggregate outputs

### 5.3 Configuration Layer

Add a Mull config file:
- `scripts/quality/mull.yml`

Responsibilities:
- `includePaths` only for the approved increment files under `src/`
- `excludePaths` for:
  - `tests/`
  - `_deps/`
  - compiler/system paths
- configure an output format that is easy to aggregate, preferably Elements-style JSON

### 5.4 Reporting Layer

Add a report aggregator:
- `scripts/quality/aggregate_mutation_reports.py`

Inputs:
- raw mutation JSON from each targeted executable

Outputs:
- `results/st/mutation_score.txt`
- `results/st/mutation_report.json`
- `results/st/mutation_survivors.txt`
- `results/st/mutation/*.json`

Responsibilities:
- merge all executable-level reports
- compute overall score and per-file stats
- emit a stable JSON shape for validators and reproducibility docs

## 6. Source-to-Test Mapping

Mutation score scope:
- `src/cli/app.cpp`
- `src/bench/runner.cpp`
- `src/core/hashprune.cpp`
- `src/core/leaf_knn.cpp`
- `src/core/pipnn_builder.cpp`
- `src/core/rbc.cpp`
- `src/search/greedy_beam.cpp`

Targeted test set:
- `test_cli_app`
- `test_cli`
- `test_hashprune`
- `test_leaf_knn`
- `test_rbc`
- `test_pipnn_integration`

Expected mapping:
- `src/cli/app.cpp` -> `test_cli_app`, `test_cli`
- `src/bench/runner.cpp` -> `test_pipnn_integration`, `test_cli`
- `src/core/hashprune.cpp` -> `test_hashprune`, `test_pipnn_integration`
- `src/core/leaf_knn.cpp` -> `test_leaf_knn`, `test_pipnn_integration`
- `src/core/rbc.cpp` -> `test_rbc`, `test_pipnn_integration`
- `src/core/pipnn_builder.cpp` -> `test_pipnn_integration`
- `src/search/greedy_beam.cpp` -> `test_pipnn_integration`

## 7. Execution Flow

### 7.1 Smoke path

1. sync repo to remote
2. ensure toolchain exists
3. configure `build-mull/`
4. build a minimal targeted executable, starting with `test_hashprune`
5. run `mull-runner` only for that executable
6. verify that at least one raw report is generated and parseable

### 7.2 Full targeted path

1. rebuild `build-mull/` cleanly
2. build all targeted executables
3. run `mull-runner` per executable
4. aggregate reports
5. emit final score and survivors
6. fetch artifacts to local `results/st/`
7. validate scored-state evidence and update docs

## 8. Failure Model

Environment failures:
- toolchain install failure -> hard fail
- `clang`/CMake configure failure -> hard fail
- individual mutation invocation failure -> hard fail

Quality failure:
- aggregate score `< 80` -> real feature failure, not blocked-state fallback

Important rule:
- once this pipeline exists, mutation scoring should not revert to implicit blocked-state for this scope; blocked-state remains only as the legacy fallback for environments where the new pipeline has not been introduced

## 9. Idempotency and Isolation

- tool installation must be reusable and skip download when already present
- `build-mull/` must be recreated from scratch on every run
- `results/st/mutation/` must be refreshed on every run to avoid stale JSON contamination
- existing `build/`, `build-cov/`, and `remote_mutation_probe.sh` remain untouched as compatibility paths

## 10. Test Strategy

### 10.1 Local harness tests

Add/extend tests for:
- `scripts/quality/aggregate_mutation_reports.py` with fixture JSON inputs
- `scripts/validate_mutation_evidence.py` for both:
  - blocked-state fixtures
  - scored-state fixtures
- `scripts/get_tool_commands.py` to ensure mutation commands point to the new runner

### 10.2 Remote smoke validation

- run only `test_hashprune`
- prove:
  - toolchain works
  - build works
  - raw report exists
  - aggregator can read the report

### 10.3 Remote acceptance

- run the full targeted set
- prove:
  - every targeted executable emits a raw report
  - aggregate score is produced
  - score is `>= 80`
  - ST report and repro manifest point to the scored-state artifacts

## 11. Documentation Updates

Update or add:
- `docs/runbooks/mutation-evidence.md`
- `results/repro_manifest.json`
- `docs/plans/2026-03-12-st-report.md`
- `task-progress.md`
- `RELEASE_NOTES.md`

The ST report must switch from:
- "blocked-state evidence recorded"

to:
- actual mutation score evidence and artifact paths

## 12. Long-Task Decomposition

Introduce three incremental features in the next wave:

### Feature 16 — Remote Mull/LLVM user-space toolchain
- remote user-space install
- version verification
- smoke availability command

### Feature 17 — Remote targeted mutation pipeline
- dedicated `build-mull/`
- per-executable mutation execution
- raw report capture and aggregation

### Feature 18 — Scored mutation evidence
- aggregate score gate `>= 80`
- scored-state validator path
- scored-state ST/repro/report updates

This split keeps failures localizable:
- 16 isolates environment/tooling
- 17 isolates execution reliability
- 18 isolates score quality and documentation

## 13. Risks and Mitigations

- LLVM/Mull version mismatch
  - mitigation: pin versions and verify before build
- Clang build incompatibility with current project settings
  - mitigation: use dedicated `build-mull/` and disable OpenMP
- excessive mutation runtime
  - mitigation: smoke first, targeted set only
- score below threshold
  - mitigation: treat as true quality failure and improve tests rather than changing policy

## 14. Definition of Done

This design is implemented when:
- remote user-space `LLVM + Mull` install is reproducible
- `bash scripts/quality/remote_mutation_run.sh` succeeds end-to-end
- targeted mutation artifacts are produced under `results/st/`
- aggregate score is at least `80`
- scored-state validation passes
- features 16, 17, and 18 are marked `passing`
- the refreshed ST report still yields a releaseable verdict with real mutation evidence
