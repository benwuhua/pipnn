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

### Fixed
- Documented that subset-scale quality evaluation must omit full `groundtruth.ivecs` when `--max-base` is used
- CLI missing-file failures now return a controlled non-zero exit instead of aborting the process
- CLI now rejects unknown options and missing option values with explicit error messages

---

_Format: [Keep a Changelog](https://keepachangelog.com/) — Updated after every git commit._
