# Feature 20 Plan - Wave 4 RBC Fidelity

## Goal
Expose RBC leaf and overlap diagnostics first, then use those diagnostics to guide any later partition-policy changes.

## Scope
- Add repo-local RBC stats structures and aggregation.
- Thread stats through `BuildRbcLeaves` and `BuildPipnnGraph`.
- Extend tests and profile output with `leaf_count / overlap / membership` evidence.
- Do not change `leaf_kNN` or `HashPrune` behavior in this step.

## Why this order
The current builder output only shows total leaf count and candidate edges. That is not enough to tell whether recall shifts come from improved overlap or from later stages. RBC diagnostics need to exist before changing leader selection or overlap policy.

## Implementation Steps
1. Add `RbcStats` and optional stats sink on `BuildRbcLeaves`.
2. Add tests for:
   - fanout=1 versus fanout>1 membership overlap accounting
   - chunk-split fallback accounting
   - builder/profile propagation of RBC diagnostics
3. Extend `PipnnBuildStats` and profile output to include RBC overlap fields.
4. Re-run local targeted tests and a remote `100k/200` quick slice using the new diagnostics.

## Files
- `src/core/rbc.h`
- `src/core/rbc.cpp`
- `src/core/pipnn_builder.h`
- `src/core/pipnn_builder.cpp`
- `src/bench/runner.cpp`
- `tests/test_rbc.cpp`
- `tests/test_pipnn_integration.cpp`
- `tests/test_runner_metrics.cpp`

## Verification
- Local:
  - `ctest --test-dir build --output-on-failure -R '^(rbc|pipnn_integration|runner_metrics)$'`
- Remote quick slice:
  - `SIFT1M_DIR=/data/work/knowhere-rs-src/data/sift ./scripts/bench/run_feature19_hashprune_100k_200.sh`
  - inspect new RBC stats in profile output
