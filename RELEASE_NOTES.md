# Release Notes — pipnn-poc

## [Unreleased]

### Added
- Initial project scaffold
- Long-task initialization artifacts (feature-list.json, guides, env bootstrap scripts, validation scripts)
- SRS and design documents for the PiPNN PoC long-task flow
- Feature 11 execution plan for the fixed SIFT1M benchmark matrix
- Feature 11 acceptance record: `docs/test-cases/feature-11-benchmark-matrix.md`
- Feature 11 runnable example: `examples/feature-11-benchmark-matrix.sh`
- Feature 12 execution plan for the recall-threshold verification
- Feature 12 acceptance record: `docs/test-cases/feature-12-recall-threshold.md`
- Feature 12 runnable example: `examples/feature-12-recall-threshold.sh`
- Feature 13 execution plan for the reproducibility manifest
- Feature 13 acceptance record: `docs/test-cases/feature-13-reproducibility.md`
- Feature 13 runnable example: `examples/feature-13-reproducibility.sh`
- Reproducibility manifest: `results/repro_manifest.json`
- Manifest validator: `scripts/validate_repro_manifest.py`
- Wave 2 quality wrappers:
  - `scripts/quality/remote_coverage.sh`
  - `scripts/quality/remote_coverage_inner.sh`
  - `scripts/quality/remote_mutation_probe.sh`
  - `scripts/quality/remote_mutation_probe_inner.sh`
- Feature 14 harness assets:
  - `scripts/validate_quality_evidence.py`
  - `tests/test_quality_evidence.cpp`
  - `examples/feature-14-coverage-workflow.sh`
  - `docs/runbooks/quality-evidence.md`
  - `docs/test-cases/feature-14-coverage-workflow.md`
- Feature 15 harness assets:
  - `scripts/validate_mutation_evidence.py`
  - `tests/test_mutation_evidence.cpp`
  - `examples/feature-15-mutation-evidence.sh`
  - `docs/test-cases/feature-15-mutation-evidence.md`
- Feature 1 closure assets:
  - `scripts/check_real_tests.py`
  - `tests/test_real_test_harness.cpp`
  - `examples/feature-1-cli-parameter-routing.sh`
  - `docs/test-cases/feature-1-cli-parameter-routing.md`
  - `docs/plans/2026-03-12-feature-1-cli-closure.md`
- Feature 16 toolchain assets:
  - `tests/test_remote_toolchain.cpp`
  - `scripts/quality/ensure_remote_mull_toolchain.sh`
  - `scripts/quality/remote_mull_toolchain_smoke.sh`
  - `scripts/quality/remote_mull_toolchain_smoke_inner.sh`
  - `docs/runbooks/mutation-evidence.md`
  - `docs/test-cases/feature-16-remote-mull-toolchain.md`
  - `examples/feature-16-remote-mull-toolchain.sh`
- Feature 17 mutation pipeline assets:
  - `tests/test_mutation_report_aggregator.cpp`
  - `scripts/quality/aggregate_mutation_reports.py`
  - `scripts/quality/mull.yml`
  - `scripts/quality/ensure_remote_mull_build_compiler.sh`
  - `scripts/quality/remote_mutation_run.sh`
  - `scripts/quality/remote_mutation_run_inner.sh`
  - `docs/test-cases/feature-17-remote-targeted-mutation-pipeline.md`
  - `examples/feature-17-remote-targeted-mutation-pipeline.sh`

### Changed
- Updated the SIFT1M benchmark report with the completed `500k/100` subset-truth PiPNN/HNSW results and corrected remote command examples
- Recorded long-task progress for the completed feature 11 benchmark cycle
- Updated the SIFT1M benchmark report with the final `beam=384` quality configuration that satisfies NFR-002 at `100k/100`, `200k/100`, and `500k/100`
- Recorded long-task progress for the completed feature 12 threshold-validation cycle
- Updated the SIFT1M benchmark report to reference the single reproducibility bundle and validator command
- Added system-testing artifacts and recorded a `Conditional-Go` verdict with quality-gate follow-up recommendations
- Reopened feature 1 for CLI hardening and corrected the long-task coverage command to measure project-owned source through `build-cov`
- Refactored the CLI entry flow into a reusable helper (`src/cli/app.cpp`) and added direct CLI parser tests
- Updated the coverage workflow to clean `build-cov` before recomputing gcovr results, preventing stale `.gcda/.gcno` mismatches
- Extended the feature 1 CLI hardening plan with typed-option diagnostics and basename-output coverage; refreshed source-only coverage evidence to line `97%` / branch `62%`
- Applied increment wave 2 for quality-gate methodology:
  - SRS now defines `NFR-005` authoritative remote x86 GCC coverage and `NFR-006` mutation score-or-blocked evidence
  - design now includes a dedicated quality-evidence workflow
  - `feature-list.json` now tracks waves and new features 14/15
  - `long-task-guide.md` and `scripts/get_tool_commands.py` now route coverage/mutation through the remote quality wrappers
- Refreshed authoritative coverage evidence from remote x86 GCC to line `95%` / branch `92%`
- Feature 14 is now mechanically enforced through a validator plus ctest coverage-harness checks instead of documentation alone
- Remote quality wrappers now exclude `results/st` during sync and reset remote `results/st` before fetch, preventing recursive artifact duplication
- Feature 15 now makes blocked mutation evidence auditable in the ST report and reproducibility manifest instead of treating missing `mull-runner` as an implicit skip
- Feature 1 is now closed under the wave-2 quality workflow, with explicit real-test marker enforcement and ST traceability across the CLI tests
- Refreshed the system-testing plan/report and current `results/st/` artifacts; project verdict is now `Go`
- Opened Wave 3 for mutation scoring: SRS/design/feature inventory now track remote user-space `LLVM + Mull`, `build-mull`, and scored-state mutation evidence via features 16/17/18
- Feature 16 is now closed with a remote-smoke-verified repo-local LLVM/Mull toolchain, including a compat `libclang-cpp17t64` path that avoids mixing the official LLVM tarball with Ubuntu's system `libLLVM-17.so.1`
- Added the execution plan for feature 17 targeted remote mutation orchestration: `docs/plans/2026-03-12-feature-17-remote-targeted-mutation-pipeline.md`
- Feature 17 is now closed with remote smoke/full raw-report generation under `results/st/mutation/`, plus a compat build compiler path at `.tools/llvm-build/current`
- `docs/runbooks/mutation-evidence.md` now documents the split between the tarball toolchain used for Mull runtime and the Ubuntu-package compiler used for `build-mull`
- `scripts/quality/remote_coverage.sh` now excludes `build-mull`, `.tools`, and `.worktrees` during sync so remote coverage no longer emits delete-noise after mutation runs
- `scripts/get_tool_commands.py` intentionally remains on the legacy mutation probe until feature 18 wires the scored-state command into the shared quality workflow

### Fixed
- Documented that subset-scale quality evaluation must omit full `groundtruth.ivecs` when `--max-base` is used
- CLI missing-file failures now return a controlled non-zero exit instead of aborting the process
- CLI now rejects unknown options and missing option values with explicit error messages
- CLI now reports option-specific invalid numeric values instead of leaking raw `stoi`/`stof` text
- CLI now accepts bare output filenames like `--output metrics.json` without requiring a parent directory component
- Mutation probe collection is now stable and auditable on both local and remote environments even when `mull-runner` is unavailable
- Coverage-harness ctests are now self-contained and no longer depend on pre-existing `results/st/*` artifacts during remote `build-cov` runs
- Remote Mull smoke now preserves `.tools/` across syncs and fetches the remote `results/st/` directory correctly after toolchain verification

---

_Format: [Keep a Changelog](https://keepachangelog.com/) — Updated after every git commit._
