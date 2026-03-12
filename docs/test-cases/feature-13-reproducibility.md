# Feature 13 Test Cases — Reproducibility

- Feature ID: 13
- Feature Title: `NFR-003 复现性`
- Date: 2026-03-12
- Tester: Codex

## Scope

Validate that one complete remote evaluation is traceable from a single local artifact and that the referenced logs and result files exist locally.

## Test Cases

### TC-13-001 — Manifest contains required environment commands

- Given the reproducibility manifest exists
- When reading `results/repro_manifest.json`
- Then it contains non-empty entries for `check_env`, `sync_repo`, `build_repo`, `test_repo`, and `fetch_logs`

Evidence:
- `results/repro_manifest.json`

### TC-13-002 — Manifest run entries resolve to local logs and local result files

- Given the manifest run entries exist
- When running `python3 scripts/validate_repro_manifest.py results/repro_manifest.json`
- Then each referenced local log and local result file exists

Evidence:
- `scripts/validate_repro_manifest.py`
- `results/repro_manifest.json`
- `remote-logs/`
- `results/`

### TC-13-003 — Manifest metrics match referenced result JSON files

- Given the manifest and result files exist
- When running `python3 scripts/validate_repro_manifest.py results/repro_manifest.json`
- Then `build_sec`, `recall_at_10`, `qps`, `edges`, and `mode` match for every tracked run

Evidence:
- `scripts/validate_repro_manifest.py`
- `results/repro_manifest.json`

## Summary

- Executed: 3
- Passed: 3
- Failed: 0
