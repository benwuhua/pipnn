# Multi-Leaf leaf_kNN Batching Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: replace the failed per-leaf exact blocked kernel with a multi-leaf batching path that preserves exact full-scan leaf semantics while reducing repeated matrix setup and scheduling cost

## 1. Goal

This iteration answers one narrow question:

Can we recover the value of batched distance computation in `leaf_kNN` by batching multiple leaves together, instead of materializing and processing one leaf at a time?

The current high-dimensional checkpoint after `RBC` batching is:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- `build_sec = 30.4769`
- `partition_sec = 12.5956`
- `leaf_knn_sec = 8.13263`
- `prune_sec = 9.74774`
- `recall_at_10 = 0.978`

The failed per-leaf exact blocked result is:

- `build_sec = 38.6283`
- `partition_sec = 12.6393`
- `leaf_knn_sec = 16.1486`
- `prune_sec = 9.83931`
- `recall_at_10 = 0.978`

That result shows the current problem is not the batched distance formula itself. The problem is the batching granularity.

## 2. Why Another Per-Leaf Tweak Is Wrong

The current evidence is already strong enough:

- `RBC` batching helped materially
- exact per-leaf blocked batching regressed almost entirely inside `leaf_knn_sec`
- recall did not move

This means the failure is architectural, not semantic.

The likely root cause is that the current leaf distribution contains many small-to-medium leaves. In that regime, paying matrix materialization, temporary allocation, and kernel dispatch cost separately for each leaf is too expensive.

Therefore the next iteration should not try a fourth per-leaf kernel variant. It should change batching granularity.

## 3. Non-Goals

This iteration does not:

- change `RBC`
- change `HashPrune`
- change search behavior
- change `scan_cap` semantics
- change candidate quality by introducing approximate leaf filtering
- create a project-wide shared distance engine
- change CLI flags unless a tiny internal debug switch becomes necessary

This is still a targeted optimization pass over one hotspot.

## 4. Proposed Approach

Recommended approach: keep exact full-scan leaf semantics, but batch multiple leaves together inside one executor.

### 4.1 High-Level Flow

Replace the current immediate per-leaf exact processing with two stages:

1. collect exact full-scan leaf jobs
2. group those jobs into batches by leaf size and batch budget
3. build one contiguous matrix per batch
4. execute exact leaf-local top-k within each leaf's subrange
5. emit the same edge format as today

Leaves do not interact semantically. Batching only changes how data is prepared and scheduled.

### 4.2 Path Split

Keep three semantic modes:

1. `naive_capped`
- current `scan_cap` path, unchanged

2. `naive_full`
- current uncapped scalar exact path, retained as fallback

3. `batched_full`
- new path for uncapped leaves that are large enough to justify batching

Only uncapped exact leaves are candidates for the new path.

## 5. Batch Construction

### 5.1 Leaf Job Collection

For each leaf that would previously go through exact full-scan:

- record the leaf's point ids
- record its size
- defer edge computation until batch execution

This means batching cannot live entirely behind the current single-leaf `BuildLeafKnnEdges(...)` call site. The batching boundary must move up one level:

- `pipnn_builder.cpp` becomes responsible for collecting exact full-scan leaf jobs
- the existing single-leaf helper remains available for capped and scalar fallback paths
- a new internal batch executor handles one or more leaf jobs and returns merged edges

### 5.2 Size Bucketing

Group leaf jobs by coarse size ranges so batches contain leaves with similar work shape.

Initial buckets can be simple and internal, for example:

- `64-127`
- `128-255`
- `256+`

Exact bucket boundaries are not part of the external contract. They only need to be stable enough for profiling and deterministic tests.

### 5.3 Batch Budget

Within each size bucket, accumulate leaves into batches until a maximum point budget is reached.

This serves two purposes:

- caps temporary memory
- prevents one oversized batch from degrading cache behavior

If adding another leaf would exceed the budget, start a new batch.

## 6. Batch Execution

### 6.1 Data Layout

For one batch:

1. concatenate all leaf vectors into one row-major matrix `X_batch`
2. keep per-leaf metadata:
   - `offset`
   - `size`
   - original global point ids
3. precompute row norms once for the whole batch

This amortizes matrix fill and temporary allocation across many leaves.

### 6.2 Distance Computation

Distance computation remains exact:

- use the same squared L2 identity:
  - `||x_i - x_j||^2 = ||x_i||^2 + ||x_j||^2 - 2 * x_i dot x_j`
- process row blocks inside the batch
- only compare rows within each leaf's subrange

No cross-leaf neighbors are allowed.

### 6.3 Top-k Semantics

For each query row:

- only consider candidate rows from the same leaf
- exclude the diagonal/self edge
- keep exact top-k nearest neighbors for that leaf
- emit edges using the original global point ids

Tie handling must remain aligned with the current exact path.

## 7. Dispatch and Fallback

### 7.1 Threshold Policy

Do not batch every uncapped leaf.

Initial policy:

- `scan_cap > 0` -> `naive_capped`
- `leaf_size < small_threshold` -> `naive_full`
- `leaf_size >= small_threshold` -> eligible for `batched_full`

`small_threshold` should be conservative at first, for example `64`.

### 7.2 Fallback Safety

Retain the current scalar exact path as a first-class fallback.

If a batch is too small, too sparse, or otherwise not worth batching, the executor should leave that work on the scalar exact path rather than forcing it into a slower kernel.

This iteration optimizes only when the conditions suggest the optimization is likely to help.

## 8. Components

### 8.1 `src/core/pipnn_builder.cpp`

Batch orchestration entry point.

Expected refactor:

- keep the existing leaf traversal loop
- split leaves into:
  - immediate capped/scalar work
  - deferred exact full-scan jobs
- dispatch deferred jobs into the multi-leaf executor
- merge returned edges back into the existing `candidates[u]` flow

### 8.2 `src/core/leaf_knn.cpp`

Primary leaf-kernel site.

Expected refactor:

- keep capped path separate and unchanged
- keep scalar exact path available
- remove the per-leaf blocked dispatch path
- keep the single-leaf exact helper as a correctness reference and fallback
- add internal batch-execution helpers used by the builder-level orchestrator

### 8.3 Small internal batching helper

If needed, extract batching-only logic into a focused helper file, for example:

- leaf job collection helpers
- batch metadata structure
- batch execution kernel

Do not introduce a generic project-wide distance layer in this iteration.

### 8.4 `tests/test_leaf_knn.cpp`

Extend tests so the batched path is forced on deterministic fixtures and compared directly against scalar exact behavior.

### 8.5 `tests/test_pipnn_integration.cpp`

Keep an integration check that the builder still produces sane graph behavior and does not change recall-oriented behavior on current fixtures.

## 9. Correctness Requirements

The optimization is allowed to change performance only.

The following must remain identical relative to the same uncapped leaf candidate set:

- neighbor identity
- self-edge exclusion
- bidirected emission behavior
- tie handling

If the scalar exact path and multi-leaf batched path disagree on the same leaf fixture, batched execution is wrong.

## 10. Testing Strategy

### 10.1 Unit / deterministic checks

Add or extend tests for:

- equality between scalar exact and multi-leaf batched exact on fixed leaves
- mixed batches containing more than one leaf
- `k >= leaf_size - 1`
- threshold boundary cases
- capped path unchanged

### 10.2 Local regression suite

Run full local `ctest` after the change.

### 10.3 Remote high-dimensional smoke

Use the existing workload:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- metric: `l2`

Primary learning metrics:

- `recall_at_10` remains `0.978` or within the same acceptable range
- `leaf_knn_sec` is materially below the failed per-leaf blocked result of `16.1486`
- total `build_sec` is materially below `38.6283`

Promotion threshold:

- the new path is eligible to replace the current checkpoint only if it also approaches or beats the post-RBC baseline:
  - `leaf_knn_sec = 8.13263`
  - `build_sec = 30.4769`

## 11. Error Handling

- clamp tiny negative distances from floating-point error to zero before comparison
- fail loudly on inconsistent batch metadata rather than silently producing wrong neighbors
- do not silently route to an approximate path on batched-path disagreement

## 12. Risks

### Risk 1: Batch management overhead still dominates

Mitigation:

- conservative thresholding
- bounded batch size
- retain scalar exact fallback

### Risk 2: Cross-leaf contamination

Mitigation:

- explicit `(offset, size)` metadata per leaf
- deterministic tests with multiple leaves in one batch

### Risk 3: Memory pressure on large batches

Mitigation:

- enforce `max_points_per_batch`
- split oversized size buckets into multiple batches

## 13. Success Criteria

This iteration is informative if all of the following hold:

- local tests pass
- deterministic tests show exact equality with the scalar path
- remote high-dimensional smoke keeps current recall behavior
- `leaf_knn_sec` is materially lower than `16.1486`
- total `build_sec` is materially lower than `38.6283`

This iteration is promotion-ready only if all of the following also hold:

- `leaf_knn_sec <= 8.13263`
- `build_sec <= 30.4769`

If the implementation is informative but not promotion-ready, it should be kept as evidence for the architecture decision and should not replace the current post-RBC baseline.

## 14. Next Decision After This Iteration

After multi-leaf batching, the next architecture decision should be based on evidence:

- if `leaf_knn_sec` improves substantially, continue refining leaf batching and only then consider a shared distance layer
- if `leaf_knn_sec` remains clearly worse than the scalar exact baseline, stop investing in exact leaf batching and move to either:
  - a broader shared distance-batching layer
  - or a different candidate-generation strategy
