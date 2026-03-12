# Release Notes — pipnn-poc

## [Unreleased]

### Added
- Initial project scaffold
- Long-task initialization artifacts (feature-list.json, guides, env bootstrap scripts, validation scripts)
- SRS and design documents for the PiPNN PoC long-task flow
- Feature 11 execution plan for the fixed SIFT1M benchmark matrix
- Feature 11 acceptance record: `docs/test-cases/feature-11-benchmark-matrix.md`
- Feature 11 runnable example: `examples/feature-11-benchmark-matrix.sh`

### Changed
- Updated the SIFT1M benchmark report with the completed `500k/100` subset-truth PiPNN/HNSW results and corrected remote command examples
- Recorded long-task progress for the completed feature 11 benchmark cycle

### Fixed
- Documented that subset-scale quality evaluation must omit full `groundtruth.ivecs` when `--max-base` is used

---

_Format: [Keep a Changelog](https://keepachangelog.com/) — Updated after every git commit._
