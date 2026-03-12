# Feature 14 Acceptance Test Cases — NFR-005 覆盖率口径固化

## Scope

Validate that the documented remote x86 GCC coverage workflow produces authoritative source-only coverage reports and that the fetched reports satisfy the configured thresholds.

## Preconditions

- `~/.config/generic-x86-remote/remote.env` is configured
- repository is at the intended revision
- `feature-list.json` contains the active quality gate thresholds

## Test Cases

### TC-14-001 Remote Coverage Wrapper
- **Given** the remote x86 GCC environment is reachable
- **When** running `bash scripts/quality/remote_coverage.sh`
- **Then** the command succeeds and refreshes:
  - `results/st/line_coverage.txt`
  - `results/st/branch_coverage.txt`

### TC-14-002 Threshold Validation
- **Given** fresh coverage reports under `results/st/`
- **When** running `python3 scripts/validate_quality_evidence.py`
- **Then** the validator exits `0`
- **And** stdout reports `line_coverage=95`
- **And** stdout reports `branch_coverage=92`

## Evidence

- Coverage wrapper: `scripts/quality/remote_coverage.sh`
- Validator: `scripts/validate_quality_evidence.py`
- Runbook: `docs/runbooks/quality-evidence.md`
