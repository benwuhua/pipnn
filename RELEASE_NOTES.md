# Release Notes — pipnn-poc

## [Unreleased]

### Added
- Initial project scaffold
- Long-task initialization artifacts (feature-list.json, guides, env bootstrap scripts, validation scripts)
- SRS and design documents for the PiPNN PoC long-task flow
- Feature 11 execution plan for the fixed SIFT1M benchmark matrix

### Changed
- Updated the SIFT1M benchmark report with `500k/100` subset-truth PiPNN results and corrected remote command examples
- Recorded long-task progress for the in-flight `500k/100` benchmark cycle

### Fixed
- Documented that subset-scale quality evaluation must omit full `groundtruth.ivecs` when `--max-base` is used

---

_Format: [Keep a Changelog](https://keepachangelog.com/) — Updated after every git commit._
