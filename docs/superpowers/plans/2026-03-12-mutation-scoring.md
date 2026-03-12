# Mutation Scoring Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current blocked-state mutation workflow with an authoritative remote x86 mutation-scoring pipeline for the approved `src/` increment set, and close `NFR-006` with a real mutation score `>= 80`.

**Architecture:** Add a repo-local user-space `LLVM + Mull` toolchain on the remote host, execute mutation in a dedicated `build-mull/` tree with `clang/clang++` and `OpenMP` disabled, and aggregate per-executable raw reports into stable scored-state artifacts under `results/st/`. Land the work as a new long-task increment split into feature 16 (toolchain), feature 17 (pipeline), and feature 18 (scored evidence), preserving the current `Go` baseline while the new path is brought up.

**Tech Stack:** C++20, CMake, clang/LLVM, Mull, Python 3, gcovr-era quality harness patterns, `generic-x86-remote` scripts, long-task increment/workflow docs.

---

## File Map

- Create: `increment-request.json`
  - signals the new mutation-scoring increment and captures scope/requirements for features 16/17/18
- Modify: `docs/plans/2026-03-12-pipnn-poc-srs.md`
  - add explicit scored-state wording for `NFR-006` after toolchain introduction
- Modify: `docs/plans/2026-03-12-pipnn-poc-design.md`
  - add the user-space LLVM/Mull toolchain layer, `build-mull/`, and scored-state report flow
- Modify: `feature-list.json`
  - add features 16/17/18 and wire their dependencies
- Modify: `task-progress.md`
  - record the increment wave and each feature session
- Create: `scripts/quality/ensure_remote_mull_toolchain.sh`
  - installs and verifies pinned LLVM/Mull under `.tools/` on the remote repo
- Create: `scripts/quality/remote_mutation_run.sh`
  - local outer wrapper that syncs, executes, and fetches scored-state artifacts
- Create: `scripts/quality/remote_mutation_run_inner.sh`
  - remote inner runner that configures `build-mull/`, builds targeted tests, executes Mull, and writes reports
- Create: `scripts/quality/mull.yml`
  - include/exclude path config for the approved increment set
- Create: `scripts/quality/aggregate_mutation_reports.py`
  - aggregates per-executable raw reports into stable score/survivor outputs
- Modify: `scripts/validate_mutation_evidence.py`
  - support both blocked-state and scored-state evidence validation
- Modify: `scripts/get_tool_commands.py`
  - point the mutation command to `remote_mutation_run.sh`
- Create: `docs/runbooks/mutation-evidence.md`
  - operator runbook for install, execution, diagnostics, and artifact layout
- Modify: `results/repro_manifest.json`
  - add scored-state mutation command, version, score, and report paths
- Modify: `docs/plans/2026-03-12-st-report.md`
  - swap blocked-state prose for real mutation score evidence
- Modify: `RELEASE_NOTES.md`
  - record new mutation pipeline and scored-state evidence
- Test: `tests/test_mutation_evidence.cpp`
  - extend to validate scored-state fixtures and preserve blocked-state coverage
- Test: `tests/test_quality_evidence.cpp`
  - unchanged behavior, but keep in regression set while mutation path evolves
- Create: `tests/test_mutation_report_aggregator.cpp`
  - fixture-driven test for `aggregate_mutation_reports.py`
- Modify: `tests/CMakeLists.txt`
  - register any new harness tests
- Create: `examples/feature-16-remote-mull-toolchain.sh`
  - smoke example for remote toolchain readiness
- Create: `examples/feature-17-remote-mutation-pipeline.sh`
  - smoke example for single-target mutation execution
- Create: `examples/feature-18-scored-mutation-evidence.sh`
  - end-to-end scored-state evidence example
- Create: `docs/test-cases/feature-16-remote-mull-toolchain.md`
- Create: `docs/test-cases/feature-17-remote-mutation-pipeline.md`
- Create: `docs/test-cases/feature-18-scored-mutation-evidence.md`

## Chunk 1: Increment Setup

### Task 1: Create the increment request and route work into the long-task workflow

**Files:**
- Create: `increment-request.json`
- Reference: `docs/superpowers/specs/2026-03-12-mutation-scoring-design.md`
- Validate: `scripts/validate_increment_request.py`

- [ ] **Step 1: Write the increment request draft**

Create `increment-request.json` with:
- summary: mutation scoring for remote x86 authoritative workflow
- requested features: 16/17/18
- scope: `src/cli/app.cpp`, `src/bench/runner.cpp`, `src/core/*`, `src/search/greedy_beam.cpp`
- constraints: remote-only authority, user-space install, targeted test set only

- [ ] **Step 2: Validate the increment request**

Run: `python3 scripts/validate_increment_request.py increment-request.json`
Expected: exit `0`

- [ ] **Step 3: Commit the increment request**

```bash
git add increment-request.json
git commit -m "docs: request mutation scoring increment"
```

### Task 2: Update long-task artifacts for the new wave

**Files:**
- Modify: `docs/plans/2026-03-12-pipnn-poc-srs.md`
- Modify: `docs/plans/2026-03-12-pipnn-poc-design.md`
- Modify: `feature-list.json`
- Modify: `task-progress.md`

- [ ] **Step 1: Add the failing feature inventory**

Update `feature-list.json` to add:
- feature 16: remote LLVM/Mull user-space toolchain
- feature 17: remote targeted mutation pipeline
- feature 18: scored mutation evidence

Set each new feature to `status: failing` and wire dependencies:
- `17` depends on `16`
- `18` depends on `17`

- [ ] **Step 2: Update SRS and design for scored-state mutation**

Extend the existing `NFR-006` and design quality workflow so they explicitly describe:
- scored-state path after toolchain provisioning
- `build-mull/`
- remote authoritative command
- score threshold `>= 80`

- [ ] **Step 3: Record the new wave in task progress**

Append a new session entry explaining that the project is re-entering long-task execution through a mutation-scoring increment.

- [ ] **Step 4: Validate the updated feature inventory**

Run: `python3 scripts/validate_features.py feature-list.json`
Expected: `OK: feature-list.json is valid`

- [ ] **Step 5: Commit the wave setup**

```bash
git add docs/plans/2026-03-12-pipnn-poc-srs.md docs/plans/2026-03-12-pipnn-poc-design.md feature-list.json task-progress.md
git commit -m "feat: add mutation scoring wave"
```

## Chunk 2: Feature 16 — Remote Mull/LLVM Toolchain

### Task 3: Add a failing harness test for scored-state validation prerequisites

**Files:**
- Modify: `tests/test_mutation_evidence.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add a new failing scored-state fixture test**

Extend `tests/test_mutation_evidence.cpp` with a fixture that expects:
- `mutation_status=scored`
- a numeric score in the manifest and ST report
- a validator success path for scored-state

The initial failure reason should be: scored-state artifacts and validator support do not exist yet.

- [ ] **Step 2: Run the targeted test to verify failure**

Run: `ctest --test-dir build --output-on-failure -R '^mutation_evidence$'`
Expected: FAIL because the scored-state path is unimplemented

- [ ] **Step 3: Commit the red test**

```bash
git add tests/test_mutation_evidence.cpp tests/CMakeLists.txt
git commit -m "test: add scored mutation evidence fixture"
```

### Task 4: Implement the remote user-space toolchain installer

**Files:**
- Create: `scripts/quality/ensure_remote_mull_toolchain.sh`
- Modify: `docs/runbooks/mutation-evidence.md`
- Create: `examples/feature-16-remote-mull-toolchain.sh`
- Create: `docs/test-cases/feature-16-remote-mull-toolchain.md`

- [ ] **Step 1: Implement the installer script**

The script must:
- pin `LLVM_VERSION` and `MULL_VERSION`
- install into `.tools/llvm/` and `.tools/mull/`
- print stable absolute paths for `clang`, `clang++`, `mull-runner`
- exit non-zero if versions are missing or incompatible

- [ ] **Step 2: Add a dry-run / print-paths mode**

Support a mode such as `--print-paths` so other scripts and tests can reuse the installer without guessing locations.

- [ ] **Step 3: Document the operator flow**

Write `docs/runbooks/mutation-evidence.md` with:
- install command
- expected tool locations
- version verification commands
- common failures and next checks

- [ ] **Step 4: Add a runnable toolchain smoke example**

`examples/feature-16-remote-mull-toolchain.sh` should:
- invoke the outer or direct remote readiness path
- print resolved tool versions and paths

- [ ] **Step 5: Add feature 16 acceptance doc**

Create `docs/test-cases/feature-16-remote-mull-toolchain.md` with at least:
- remote install/readiness test case
- path/version verification test case

- [ ] **Step 6: Run local regression**

Run: `ctest --test-dir build --output-on-failure -R '^mutation_evidence$'`
Expected: still FAIL or partially FAIL until validator support lands, but no regressions outside the new scored fixture should appear

- [ ] **Step 7: Run remote smoke manually**

Run on remote through the generic-x86 wrapper:
- installer script
- `mull-runner --version`
- `clang++ --version`

Expected: all commands succeed from the repo-local `.tools/` tree

- [ ] **Step 8: Commit feature 16 implementation**

```bash
git add scripts/quality/ensure_remote_mull_toolchain.sh docs/runbooks/mutation-evidence.md examples/feature-16-remote-mull-toolchain.sh docs/test-cases/feature-16-remote-mull-toolchain.md
git commit -m "feat: add remote mull toolchain installer"
```

## Chunk 3: Feature 17 — Remote Targeted Mutation Pipeline

### Task 5: Add a failing aggregator harness test

**Files:**
- Create: `tests/test_mutation_report_aggregator.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write fixture-based failing tests for report aggregation**

Use synthetic Elements-style JSON inputs to assert that the aggregator emits:
- total mutants
- killed mutants
- survived mutants
- aggregate score
- per-file stats

- [ ] **Step 2: Run the targeted test to verify failure**

Run: `ctest --test-dir build --output-on-failure -R '^mutation_report_aggregator$'`
Expected: FAIL because the aggregator script is not implemented

- [ ] **Step 3: Commit the red test**

```bash
git add tests/test_mutation_report_aggregator.cpp tests/CMakeLists.txt
git commit -m "test: add mutation report aggregator fixture"
```

### Task 6: Implement the mutation runner and aggregator

**Files:**
- Create: `scripts/quality/remote_mutation_run.sh`
- Create: `scripts/quality/remote_mutation_run_inner.sh`
- Create: `scripts/quality/mull.yml`
- Create: `scripts/quality/aggregate_mutation_reports.py`
- Modify: `scripts/get_tool_commands.py`
- Create: `examples/feature-17-remote-mutation-pipeline.sh`
- Create: `docs/test-cases/feature-17-remote-mutation-pipeline.md`

- [ ] **Step 1: Implement the outer remote wrapper**

`remote_mutation_run.sh` must:
- sync the repo
- call the remote installer
- trigger smoke mode first
- trigger full targeted mode second
- fetch `results/st/mutation*` artifacts

- [ ] **Step 2: Implement the inner remote runner**

`remote_mutation_run_inner.sh` must:
- recreate `build-mull/`
- configure CMake with repo-local `clang/clang++`
- pass `-DPIPNN_ENABLE_OPENMP=OFF`
- build only the targeted executables
- run `mull-runner` per executable
- write raw reports under `results/st/mutation/`

- [ ] **Step 3: Add the Mull config**

`scripts/quality/mull.yml` must:
- include only the approved increment files
- exclude tests, `_deps`, and non-project paths
- choose JSON output suitable for aggregation

- [ ] **Step 4: Implement the aggregator**

`scripts/quality/aggregate_mutation_reports.py` must emit:
- `results/st/mutation_score.txt`
- `results/st/mutation_report.json`
- `results/st/mutation_survivors.txt`

- [ ] **Step 5: Point the tool command map to the new runner**

Update `scripts/get_tool_commands.py` so `mutation:` resolves to `bash scripts/quality/remote_mutation_run.sh`.

- [ ] **Step 6: Add the pipeline example and acceptance doc**

`examples/feature-17-remote-mutation-pipeline.sh` should run smoke mode and validate that at least one raw report exists.

`docs/test-cases/feature-17-remote-mutation-pipeline.md` should cover:
- smoke execution
- full targeted execution
- artifact fetch verification

- [ ] **Step 7: Run the local aggregator test**

Run: `ctest --test-dir build --output-on-failure -R '^(mutation_report_aggregator|mutation_evidence)$'`
Expected: aggregator test PASS; scored-state validator test may still fail until feature 18 documentation updates land

- [ ] **Step 8: Run remote smoke**

Run: `bash scripts/quality/remote_mutation_run.sh`
Expected smoke milestone:
- `results/st/mutation/*.json` exists for `test_hashprune`
- `results/st/mutation_report.json` exists

- [ ] **Step 9: Commit feature 17 implementation**

```bash
git add scripts/quality/remote_mutation_run.sh scripts/quality/remote_mutation_run_inner.sh scripts/quality/mull.yml scripts/quality/aggregate_mutation_reports.py scripts/get_tool_commands.py examples/feature-17-remote-mutation-pipeline.sh docs/test-cases/feature-17-remote-mutation-pipeline.md tests/test_mutation_report_aggregator.cpp tests/CMakeLists.txt
git commit -m "feat: add remote mutation pipeline"
```

## Chunk 4: Feature 18 — Scored Mutation Evidence

### Task 7: Extend the validator and documentation to support scored-state

**Files:**
- Modify: `scripts/validate_mutation_evidence.py`
- Modify: `tests/test_mutation_evidence.cpp`
- Modify: `results/repro_manifest.json`
- Modify: `docs/plans/2026-03-12-st-report.md`
- Create: `examples/feature-18-scored-mutation-evidence.sh`
- Create: `docs/test-cases/feature-18-scored-mutation-evidence.md`

- [ ] **Step 1: Implement scored-state validation**

Extend `scripts/validate_mutation_evidence.py` so it accepts:
- `blocked` status with follow-up action
- `scored` status with numeric `score`, raw report paths, and ST report references

- [ ] **Step 2: Make the scored fixture test pass**

Update `tests/test_mutation_evidence.cpp` fixtures and expected outputs to cover both states.

- [ ] **Step 3: Update the reproducibility manifest**

Write scored-state fields into `results/repro_manifest.json`, including:
- command
- tool versions
- score
- report paths
- survivor summary path

- [ ] **Step 4: Update ST report language**

Replace blocked-state wording in `docs/plans/2026-03-12-st-report.md` with:
- measured mutation score
- report locations
- verdict impact

- [ ] **Step 5: Add feature 18 example and acceptance doc**

`examples/feature-18-scored-mutation-evidence.sh` should:
- run `remote_mutation_run.sh`
- run `validate_mutation_evidence.py`
- print the aggregate score

`docs/test-cases/feature-18-scored-mutation-evidence.md` should cover:
- aggregate score generation
- validator success
- ST/report/repro linkage

- [ ] **Step 6: Run local regression for harness tests**

Run: `ctest --test-dir build --output-on-failure -R '^(mutation_evidence|mutation_report_aggregator)$'`
Expected: PASS

- [ ] **Step 7: Run the authoritative remote mutation workflow**

Run: `bash scripts/quality/remote_mutation_run.sh`
Expected:
- all targeted executables emit raw reports
- `results/st/mutation_score.txt` exists
- aggregate score is numeric and `>= 80`

- [ ] **Step 8: Validate scored-state evidence**

Run:
- `python3 scripts/validate_mutation_evidence.py`
- `python3 scripts/validate_repro_manifest.py results/repro_manifest.json`

Expected: both exit `0`

- [ ] **Step 9: Commit feature 18 implementation**

```bash
git add scripts/validate_mutation_evidence.py tests/test_mutation_evidence.cpp results/repro_manifest.json docs/plans/2026-03-12-st-report.md examples/feature-18-scored-mutation-evidence.sh docs/test-cases/feature-18-scored-mutation-evidence.md
git commit -m "feat: add scored mutation evidence"
```

## Chunk 5: Refresh Quality, ST, and Persist

### Task 8: Re-run quality and system-testing evidence after scored-state lands

**Files:**
- Modify: `task-progress.md`
- Modify: `RELEASE_NOTES.md`
- Modify: `feature-list.json`

- [ ] **Step 1: Run the full local regression suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: all tests pass

- [ ] **Step 2: Run authoritative remote quality commands**

Run:
- `bash scripts/quality/remote_coverage.sh`
- `bash scripts/quality/remote_mutation_run.sh`

Expected:
- coverage remains `>= 90 / 80`
- mutation score is `>= 80`

- [ ] **Step 3: Refresh ST artifacts and examples**

Run:
- `./examples/feature-16-remote-mull-toolchain.sh`
- `./examples/feature-17-remote-mutation-pipeline.sh`
- `./examples/feature-18-scored-mutation-evidence.sh`

Expected: all pass and write fresh evidence paths referenced by the ST report

- [ ] **Step 4: Mark features 16, 17, 18 passing**

Update `feature-list.json` with:
- `status: passing`
- `st_case_path`
- `st_case_count`

- [ ] **Step 5: Update progress and release notes**

Record the scored-state completion in:
- `task-progress.md`
- `RELEASE_NOTES.md`

- [ ] **Step 6: Validate inventory and whitespace**

Run:
- `python3 scripts/validate_features.py feature-list.json`
- `python3 scripts/check_st_readiness.py feature-list.json`
- `git diff --check`

Expected:
- feature list valid
- ST readiness returns `READY`
- no whitespace issues

- [ ] **Step 7: Commit the status update**

```bash
git add feature-list.json task-progress.md RELEASE_NOTES.md
git commit -m "chore: mark mutation scoring features passing"
```

- [ ] **Step 8: Push the branch**

```bash
git push origin main
```
