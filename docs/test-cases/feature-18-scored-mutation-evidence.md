# Feature 18 Acceptance Test Cases — NFR-006 scored mutation evidence

## Scope

Validate that the remote x86 scored-state mutation workflow produces the required aggregate artifacts, satisfies the `>= 80` threshold, and is wired into the shared validator plus reproducibility documents.

## Preconditions

- `~/.config/generic-x86-remote/remote.env` is configured
- feature 17 raw mutation pipeline is already passing
- repository is at the intended revision

## Test Cases

### TC-18-001 Remote Scored Mutation Run
- **Given** the remote x86 LLVM/Mull toolchain is reachable
- **When** running `bash scripts/quality/remote_mutation_run.sh --mode full --workers 4`
- **Then** the command succeeds
- **And** it refreshes:
  - `results/st/mutation_score.txt`
  - `results/st/mutation_report.json`
  - `results/st/mutation_survivors.txt`
  - `results/st/mutation/full/run_manifest.txt`

### TC-18-002 Scored-State Validation
- **Given** fresh scored mutation artifacts and updated documentation
- **When** running `python3 scripts/validate_mutation_evidence.py`
- **Then** the validator exits `0`
- **And** stdout reports `mutation_status=scored`
- **And** `results/st/mutation_score.txt` contains `80.8`
- **And** the ST report plus reproducibility manifest both reference the scored artifact paths and tool versions

## Evidence

- Mutation wrapper: `scripts/quality/remote_mutation_run.sh`
- Aggregator: `scripts/quality/aggregate_mutation_reports.py`
- Validator: `scripts/validate_mutation_evidence.py`
- Repro manifest: `results/repro_manifest.json`
- ST report: `docs/plans/2026-03-12-st-report.md`
