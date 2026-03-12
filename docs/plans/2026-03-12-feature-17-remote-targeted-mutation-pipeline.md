# Feature Plan — Feature 17 Remote Targeted Mutation Pipeline

- Date: 2026-03-12
- Feature: `#17 NFR-006 远端 targeted mutation pipeline`
- Scope: add the remote `build-mull + targeted mull-runner` orchestration that produces raw mutation reports for smoke and full modes, without yet aggregating them into the scored-state artifacts required by feature 18.

## Design Alignment

- Follow design section `4.4` by keeping the remote x86 host as the only authoritative mutation runtime.
- Reuse feature 16's repo-local `.tools/` installer and cached toolchain; do not reintroduce ad hoc remote commands.
- Stop at raw report generation and fetch:
  - smoke: `test_hashprune`
  - full: the targeted executable set
- Do not update ST report or reproducibility manifest here; that belongs to feature 18.

## Harness-First Work

1. Add an aggregator fixture test for Elements-style raw reports so feature 18 has a stable downstream contract.
2. Add repo-local wrappers for:
   - remote outer run
   - remote inner run
   - raw report aggregation
3. Update `scripts/get_tool_commands.py` only when the remote mutation run command is actually usable.

## Tasks

1. Add `tests/test_mutation_report_aggregator.cpp` and register the new ctest target.
2. Implement `scripts/quality/aggregate_mutation_reports.py` against synthetic JSON fixtures.
3. Implement:
   - `scripts/quality/remote_mutation_run.sh`
   - `scripts/quality/remote_mutation_run_inner.sh`
4. Add smoke/full mode selection and targeted executable list handling.
5. Emit and fetch raw reports under `results/st/mutation/`.
6. Add `examples/feature-17-remote-targeted-mutation-pipeline.sh` and `docs/test-cases/feature-17-remote-targeted-mutation-pipeline.md`.
7. Run:
   - local ctest for the new aggregator harness
   - remote smoke mode for `test_hashprune`
   - remote full mode for the targeted set

## Files

- `tests/test_mutation_report_aggregator.cpp`
- `tests/CMakeLists.txt`
- `scripts/quality/aggregate_mutation_reports.py`
- `scripts/quality/remote_mutation_run.sh`
- `scripts/quality/remote_mutation_run_inner.sh`
- `examples/feature-17-remote-targeted-mutation-pipeline.sh`
- `docs/test-cases/feature-17-remote-targeted-mutation-pipeline.md`
