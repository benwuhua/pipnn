# Quality Evidence Runbook

## Purpose

Use this runbook when a feature cycle or system test needs authoritative coverage evidence for the PiPNN PoC.

## Coverage Workflow

From the repository root:

```bash
bash scripts/quality/remote_coverage.sh
python3 scripts/validate_quality_evidence.py
```

Expected outputs:

- `results/st/line_coverage.txt`
- `results/st/branch_coverage.txt`

Expected thresholds come from `feature-list.json`:

- `line_coverage_min`
- `branch_coverage_min`

Branch evidence is measured from remote x86 GCC and excludes compiler-generated throw/unreachable branches.

## Mutation Probe

From the repository root:

```bash
bash scripts/quality/remote_mutation_probe.sh
```

Expected outputs:

- `results/st/mutation_probe_local.txt`
- `results/st/remote/mutation_probe_remote.txt`

If `mull-runner` is unavailable, the probe remains valid evidence and should be referenced by the current feature or ST report instead of being silently skipped.

For the remote LLVM + Mull scored-state toolchain workflow, use:

- `docs/runbooks/mutation-evidence.md`
