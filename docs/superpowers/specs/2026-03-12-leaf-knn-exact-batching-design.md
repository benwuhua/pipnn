# Exact leaf_kNN Batching Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: reduce `leaf_knn` cost with an exact batched kernel while preserving current candidate semantics and the post-RBC-batching builder behavior

## 1. Goal

This iteration answers one narrow question:

Can we materially reduce `leaf_knn_sec` by batching exact leaf-internal distance computation, without changing graph semantics or recall?

The current high-dimensional checkpoint after `RBC` batching is:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- `build_sec = 30.4769`
- `partition_sec = 12.5956`
- `leaf_knn_sec = 8.13263`
- `prune_sec = 9.74774`
- `recall_at_10 = 0.978`

After `RBC` batching, `leaf_knn` is no longer the largest phase, but it is still large enough to justify a focused optimization pass.

## 2. Why This Iteration Stays Exact

The goal is to isolate batching value, not to change graph quality.

Therefore this iteration explicitly preserves current leaf semantics:

- for uncapped leaves, compare every point against every other point in the leaf
- for capped leaves, preserve the current capped scan behavior
- keep exact top-k selection over the candidate set seen by the current code
- keep self-edge exclusion and bidirected edge emission unchanged

If recall changes in this iteration, that indicates a bug.

## 3. Non-Goals

This iteration does not:

- change `RBC`
- change `HashPrune`
- change `beam` search
- introduce approximate candidate generation inside leaves
- introduce a new shared distance engine for the whole project
- change CLI flags or benchmark routing unless the wrapper needs a tiny usability adjustment

## 4. Current Hot Path

Today `BuildLeafKnnEdges(...)` does this for each point `u` in a leaf:

1. iterate candidate `v` values one by one
2. compute `L2Squared(points[u], points[v])`
3. collect all `(distance, v)` pairs into a vector
4. run `partial_sort`
5. emit the first `k` edges

This is exact, but it is scalar and repeatedly reloads the same high-dimensional leaf vectors.

On the current high-dimensional smoke, this stage still consumes more than eight seconds on `5k/50`.

## 5. Proposed Approach

Recommended approach: keep the existing public interface and replace only the exact full-scan inner kernel with an `Eigen` blocked implementation.

### 5.1 Path Split

Keep three semantic modes inside `BuildLeafKnnEdges(...)`:

1. `naive_full`
- current exact full scan over the whole leaf

2. `naive_capped`
- current scan-cap behavior

3. `blocked_full`
- new exact full-scan path for leaves large enough to benefit from batching

Only `naive_full` is replaced by `blocked_full` when thresholds say it is worthwhile.

### 5.2 blocked_full Algorithm

For one leaf:

1. materialize a row-major matrix `X` for all leaf vectors
2. precompute row norms for `X`
3. process query rows in blocks
4. compute `X_block * X^T`
5. recover squared distances via:
   - `||x_i - x_j||^2 = ||x_i||^2 + ||x_j||^2 - 2 * x_i dot x_j`
6. skip diagonal self edges
7. maintain exact top-k nearest neighbors for each query row
8. emit edges exactly as today

### 5.3 Top-k Policy

Do not sort the entire row if it is not needed.

For each query row, maintain a temporary vector of `(distance, local_index)` pairs and use the current exact selection pattern at the row level. A small row-local `partial_sort` or equivalent exact top-k selection is acceptable.

This iteration is about batching distance computation, not inventing a new top-k data structure.

### 5.4 Dispatch Policy

Small leaves should continue to use the current scalar path.

Blocked full-scan activates only when:

- `scan_cap == 0` or otherwise uncapped
- leaf size is above an internal threshold

Thresholds may remain internal constants for now.

## 6. Correctness Requirements

The new exact batched path must preserve:

- candidate set identity for uncapped leaves
- exact top-k identity relative to the same candidate set
- self-edge exclusion
- bidirected edge behavior

Blocked full-scan and naive full-scan must produce the same neighbor edges on deterministic fixtures.

## 7. Components

### 7.1 `src/core/leaf_knn.cpp`

Primary change site.

Expected refactor:

- extract current scalar full-scan path into a named helper
- keep capped path separate
- add blocked full-scan helper using `Eigen`
- dispatch between them by internal thresholds

### 7.2 Optional small helper file

If `leaf_knn.cpp` grows too large, extract only the batching-specific buffer fill and row-level top-k helper into a tiny internal file. Do not create a project-wide math layer yet.

### 7.3 `tests/test_leaf_knn.cpp`

Extend tests so exact blocked and exact scalar full-scan paths are compared directly.

### 7.4 Existing benchmark wrapper

Reuse:

- `scripts/bench/run_simplewiki_openai_20k_100.sh`

Primary validation stays on:

- `MAX_BASE=5000`
- `MAX_QUERY=50`

Optional follow-up after success:

- `MAX_BASE=20000`
- `MAX_QUERY=100`

## 8. Testing Strategy

### 8.1 Unit / deterministic checks

Add tests for:

- exact equality between scalar full-scan and blocked full-scan on fixed leaves
- diagonal/self exclusion
- `k >= leaf_size - 1`
- threshold boundary where dispatch changes from scalar to blocked
- capped path unchanged

### 8.2 Existing regression suite

Run full local `ctest` after the change.

### 8.3 Remote high-dimensional smoke

Use the same `simplewiki-openai 5k/50` workload.

Primary success metrics:

- `leaf_knn_sec` lower than `8.13263`
- total `build_sec` lower than `30.4769`
- `recall_at_10` remains `0.978` or within the same acceptable range with no semantic regression evidence

Secondary signal:

- if `leaf_knn_sec` drops but total `build_sec` barely moves, then `prune` becomes the next dominant stage and the next iteration should move there or broaden batching scope

## 9. Risks

### Risk 1: Matrix setup overhead dominates moderate leaves

Mitigation:

- keep scalar fallback for small leaves
- gate blocked path behind internal thresholds

### Risk 2: Exact top-k result changes because of row-local ordering differences

Mitigation:

- compare blocked edges directly against scalar edges in deterministic tests
- keep tie handling explicit when distances are equal

### Risk 3: improvement is too small after RBC batching

Mitigation:

- use the same high-dimensional smoke slice
- compare stage timings directly against the post-RBC baseline

## 10. Success Criteria

This iteration is successful if all of the following hold:

- local tests pass
- blocked full-scan matches scalar full-scan on deterministic tests
- remote `simplewiki-openai 5k/50` smoke keeps current recall
- `leaf_knn_sec` is materially below `8.13263`
- total `build_sec` is below `30.4769`

## 11. Next Decision After This Iteration

After exact leaf batching, the next choice becomes straightforward:

- if `leaf_knn_sec` drops materially, continue with the staged batching plan and only then consider a larger shared distance layer
- if `leaf_knn_sec` does not drop enough, the current per-stage batching approach has reached diminishing returns and the next design should move to a broader shared distance-batching layer
