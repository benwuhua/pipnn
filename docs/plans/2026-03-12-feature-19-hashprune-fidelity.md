# Feature 19 Plan - Wave 4 HashPrune Fidelity

## Goal
Implement paper-oriented HashPrune semantics and diagnostics without changing RBC or leaf_kNN behavior.

## Constraints
- Scope is limited to `HashPrune` semantics and prune diagnostics.
- Do not mix RBC or leaf_kNN optimization into this feature.
- Stage evidence must preserve `Recall@10 >= 0.95` on `100k/200` after the feature lands.

## Design Inputs
- SRS: `NFR-007 Paper Fidelity`
- Design: `4.5 特性E: HashPrune Fidelity`
- Existing contract: one best candidate per hash bucket, global degree bound, deterministic tie-break, order-independent output.

## Implementation Steps
1. Add prune diagnostics structs for per-node and build-level aggregation.
2. Write failing unit tests for:
   - bucket occupancy and same-bucket replacement accounting
   - full-capacity cross-bucket eviction versus drop behavior
   - deterministic tie-break accounting and order independence
   - build/profile stats exposing kept/dropped prune counters
3. Implement minimal HashPrune changes:
   - optional stats sink on `PruneNode`
   - deterministic accounting for kept/replaced/evicted/dropped candidates
   - builder aggregation into `PipnnBuildStats`
4. Extend profile output so prune diagnostics are visible in both builder and runner paths.
5. Re-run targeted local tests, then run the `100k/200` remote slice to confirm recall and diagnostics.

## Files
- `src/core/hashprune.h`
- `src/core/hashprune.cpp`
- `src/core/pipnn_builder.h`
- `src/core/pipnn_builder.cpp`
- `src/bench/runner.cpp`
- `tests/test_hashprune.cpp`
- `tests/test_pipnn_integration.cpp`
- `tests/test_runner_metrics.cpp`

## Verification
- Local:
  - `ctest --test-dir build --output-on-failure -R '^(hashprune|pipnn_integration|runner_metrics)$'`
- Remote authority for this feature:
  - `bash scripts/bench/remote_bench_sift1m.sh` with `100k/200` feature-19 params
  - confirm `Recall@10 >= 0.95`
  - confirm prune diagnostics are present in profile/log output
