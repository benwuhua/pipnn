# Feature 15 Acceptance Test Cases — NFR-006 mutation 证据与阻塞态审计

## Scope

Validate that mutation probe evidence is collected from both local and remote environments, and that the ST report plus reproducibility manifest reference the current blocked-state evidence and follow-up action.

## Preconditions

- `~/.config/generic-x86-remote/remote.env` is configured
- repository is at the intended revision

## Test Cases

### TC-15-001 Mutation Probe Collection
- **Given** local and remote x86 environments
- **When** running `bash scripts/quality/remote_mutation_probe.sh`
- **Then** the command succeeds
- **And** it refreshes:
  - `results/st/mutation_probe_local.txt`
  - `results/st/remote/mutation_probe_remote.txt`

### TC-15-002 Mutation Evidence Validation
- **Given** fresh mutation probe logs and updated documentation
- **When** running `python3 scripts/validate_mutation_evidence.py`
- **Then** the validator exits `0`
- **And** stdout reports `mutation_status=blocked`
- **And** the ST report plus reproducibility manifest both reference blocked-state evidence and follow-up action

## Evidence

- Probe wrapper: `scripts/quality/remote_mutation_probe.sh`
- Validator: `scripts/validate_mutation_evidence.py`
- ST report: `docs/plans/2026-03-12-st-report.md`
- Repro manifest: `results/repro_manifest.json`
