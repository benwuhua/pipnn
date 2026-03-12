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

### Fixed
- Documented that subset-scale quality evaluation must omit full `groundtruth.ivecs` when `--max-base` is used
- CLI missing-file failures now return a controlled non-zero exit instead of aborting the process
- CLI now rejects unknown options and missing option values with explicit error messages
- CLI now reports option-specific invalid numeric values instead of leaking raw `stoi`/`stof` text
- CLI now accepts bare output filenames like `--output metrics.json` without requiring a parent directory component
- Mutation probe collection is now stable and auditable on both local and remote environments even when `mull-runner` is unavailable

---

_Format: [Keep a Changelog](https://keepachangelog.com/) — Updated after every git commit._
