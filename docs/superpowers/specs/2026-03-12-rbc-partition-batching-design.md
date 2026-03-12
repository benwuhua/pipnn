# RBC Partition Batching Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: reduce the dominant `RBC partition` cost exposed by the high-dimensional validation run, without changing the current tree semantics or leaf-overlap policy

## 1. Goal

This design exists to answer one narrow question:

Can we materially reduce `partition_sec` by batching point-to-leader distance computation inside `RBC`, while keeping the current `Feature 20` tree behavior unchanged?

The immediate target is the high-dimensional workload discovered during validation:

- dataset path: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- dimension: `3072`

Current checkpoint on that slice:

- `PiPNN build_sec = 39.0241`
- `partition_sec = 18.4711`
- `leaf_knn_sec = 9.34212`
- `prune_sec = 11.2098`
- `recall_at_10 = 0.978`

The dominant stage is now `partition`, not `leaf_knn`.

## 2. Why This Comes Before leaf_kNN

The high-dimensional smoke changed the earlier diagnosis.

On `SIFT1M`, `leaf_knn` looked like the obvious next hotspot. On `simplewiki-openai 3072d`, the largest stage cost moved into `RBC` assignment. That means a leaf-only optimization would not address the dominant cost on the workload that is closer to the paper's high-dimensional regime.

Therefore the next iteration should optimize the first large distance-heavy phase before returning to `leaf_kNN`.

## 3. Non-Goals

This iteration does not:

- change leader sampling policy
- change recursive tree structure
- change leaf-overlap semantics
- change `HashPrune`
- change `leaf_kNN`
- introduce new dataset routing or reader logic
- attempt a full shared distance-engine abstraction

This is a targeted optimization pass over one hotspot.

## 4. Current Hot Path

Today `rbc.cpp` assigns each point in the node to its nearest leader like this:

1. sample `leader_count`
2. for each point in `ids`
3. compute `L2Squared(point, leader_i)` for every leader
4. choose the nearest leader with the current tie-break
5. append the point to that bucket

This is scalar and repeatedly re-reads high-dimensional vectors.

In high dimension, that cost is large enough that `partition_sec` becomes the slowest stage.

## 5. Proposed Approach

Recommended approach: keep the existing recursive `RBC` algorithm, but replace scalar point-to-leader assignment with a blocked `Eigen` kernel.

### 5.1 Data Path

For each internal `RBC` node:

1. materialize a row-major matrix `X` for the node points referenced by `ids`
2. materialize a row-major matrix `L` for the sampled leaders
3. precompute squared norms for rows of `X`
4. precompute squared norms for rows of `L`
5. compute distances in blocks using:
   - `D(x, l) = ||x||^2 + ||l||^2 - 2 * x dot l`
6. preserve the existing nearest-leader selection and tie-break
7. emit the same buckets as the scalar implementation

### 5.2 Blocking Policy

The first implementation should be simple and explicit:

- block over point rows
- keep all leaders of the current node in one matrix when practical
- use a configurable internal block size constant
- prefer row-major buffers to reduce surprise in fill order

This is intentionally local to `RBC`. No attempt is made to generalize batching across the whole codebase in this iteration.

### 5.3 Fallback Policy

Not every node should pay matrix setup cost.

The implementation should keep a scalar fallback for small nodes. The blocked path should activate only when both of these are true:

- node point count exceeds a minimum threshold
- leader count exceeds a minimum threshold

The thresholds may start as internal constants. This iteration does not need new CLI flags unless profiling proves they are necessary.

## 6. Correctness Requirements

The optimization is allowed to change performance only. It is not allowed to change observable `RBC` behavior.

The following must remain identical to the scalar path:

- leader set for a fixed RNG seed
- nearest-leader selection result
- tie-break on equal distances
- bucket membership
- resulting leaf statistics and overlap statistics

If blocked and scalar assignment disagree on the same inputs, blocked assignment is wrong.

## 7. Components

### 7.1 `src/core/rbc.cpp`

Primary change site.

Expected refactor:

- keep `BuildNode(...)` as the semantic entry point
- extract scalar assignment into a named helper
- add blocked assignment helper using `Eigen`
- route between scalar and blocked helpers based on node size

### 7.2 Optional small internal helper

If `rbc.cpp` grows too much, extract only the batching-specific buffer fill and block distance helper into a tiny internal file. Do not create a generic shared math layer in this iteration.

### 7.3 `tests/test_rbc.cpp`

Extend tests so blocked assignment is forced on deterministic synthetic cases and checked against the scalar result.

### 7.4 Benchmark wrapper reuse

Reuse the existing high-dimensional validation wrapper:

- `scripts/bench/run_simplewiki_openai_20k_100.sh`

First validation should stay on:

- `MAX_BASE=5000`
- `MAX_QUERY=50`

## 8. Testing Strategy

### 8.1 Unit / deterministic checks

Add tests that verify blocked assignment matches scalar assignment for:

- small fixed 2D and 3D point sets
- equal-distance tie cases
- nodes with more than one leader
- threshold boundary cases where scalar and blocked paths switch

### 8.2 Existing regression suite

Run full local `ctest` after the change.

### 8.3 Remote high-dimensional smoke

Use `crisp/simplewiki-openai` with the existing wrapper.

Primary success metrics:

- `partition_sec` lower than `18.4711`
- total `build_sec` lower than `39.0241`
- `recall_at_10` remains in the same acceptable range as the current smoke result

Secondary signal:

- if `partition_sec` drops but total `build_sec` barely moves, then `prune` becomes the next bottleneck and the architecture decision shifts accordingly

## 9. Error Handling

- if `Eigen` path produces negative distances from floating-point error, clamp at zero before comparison if needed
- if blocked path allocation fails or dimensions are inconsistent, fail loudly with a diagnostic rather than silently degrading correctness
- do not silently fall back to a different semantic path on mismatches

## 10. Risks

### Risk 1: Setup overhead dominates small nodes

Mitigation:

- keep scalar fallback
- gate blocked path behind internal thresholds

### Risk 2: Blocked path changes tie behavior

Mitigation:

- test exact bucket identity against scalar logic
- preserve explicit tie-break by leader id or index exactly as today

### Risk 3: Improvement is limited by later stages

Mitigation:

- keep phase timing output unchanged
- use the same smoke wrapper so partition/leaf/prune can still be compared directly

## 11. Success Criteria

This iteration is successful if all of the following hold:

- local tests pass
- blocked assignment matches scalar assignment on deterministic tests
- remote `simplewiki-openai 5k/50` smoke still produces acceptable recall
- `partition_sec` is materially below `18.4711`
- total `build_sec` is below `39.0241`

## 12. Next Decision After This Iteration

After the `RBC` batching run, only one question remains:

- if `partition` drops substantially, continue and apply batching to `leaf_kNN`
- if `partition` does not drop enough, then the current per-node materialization strategy is insufficient and the next design should move to a broader shared distance-batching layer
