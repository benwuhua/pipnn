# Feature 21 Design — Blocked leaf_kNN Kernel

- Date: 2026-03-12
- Status: Draft for user review
- Scope: replace the current per-point leaf scan in `leaf_kNN` with a blocked distance kernel that is closer to the paper's batch-compute direction while preserving the feature-19/20 graph-building semantics

## 1. Goal

Feature 21 exists to reduce the dominant `leaf_kNN` cost after the wave-4 `HashPrune` and `RBC` fidelity fixes.

The current best wave-4 checkpoint is:
- dataset: `SIFT1M`
- slice: `100k/200`
- config: `fanout=2`, `replicas=1`
- result: `build_sec=68.4892`, `recall_at_10=0.995`
- profile:
  - `partition_sec=27.8396`
  - `leaf_knn_sec=30.8155`
  - `prune_sec=9.8123`

At this point the largest remaining stage cost is `leaf_knn_sec`, so this feature focuses only on leaf-level candidate generation.

## 2. Success Criteria

Feature 21 is complete when all of the following are true:
- the wave-4 `HashPrune` and `RBC` semantics remain unchanged
- `BuildLeafKnnEdges(...)` still produces the same edge semantics for the full-scan path
- on `SIFT1M 100k/200`, completed feature-21 builds recover to `Recall@10 >= 0.95`
- `leaf_knn_sec` is materially lower than the current `30.8155s` checkpoint
- total `build_sec` is lower than the current `68.4892s` checkpoint
- the implementation remains compatible with the existing prune/search/integration pipeline

## 3. Constraints

- Do not mix this optimization with new `HashPrune` or `RBC` semantic changes.
- Do not change the public CLI contract just to support the kernel.
- Do not widen this feature into a full builder rewrite.
- Do not require BLAS/OpenBLAS as a hard dependency in this round.
- Keep the current fallback path available for small leaves and capped scans.

## 4. Recommended Approach

### Option A — Recommended: Eigen-backed blocked full-scan kernel

- Keep the public `BuildLeafKnnEdges(...)` interface stable.
- Preserve the current exact full-scan semantics when `scan_cap <= 0`.
- Internally add a blocked path that:
  - materializes leaf points into a compact local matrix
  - precomputes row norms
  - evaluates distances in `query_block x candidate_block` tiles via matrix multiply / dot-product batches
  - merges tile results into per-query top-k state
- Use a small-leaf fallback threshold so tiny leaves keep the current scalar implementation.

Why this is recommended:
- closest to the paper's batch-compute direction without introducing a heavy external runtime
- isolates the change to `leaf_kNN`
- preserves the builder semantics already stabilized by features 19 and 20
- gives a clear place to measure speedup: only `leaf_knn_sec`

### Option B — Hand-written blockwise kernel

- Implement tiled loops and dot-product accumulation manually with OpenMP.

Trade-off:
- avoids new dependency management, but increases implementation and debugging complexity without a clear short-term advantage over Eigen

### Option C — External BLAS backend

- route tile multiplies through OpenBLAS or another BLAS provider

Trade-off:
- highest theoretical ceiling, but adds environment complexity and weakens the current PoC reproducibility story

## 5. Architecture

### 5.1 Public API Boundary

Keep the current external signature:

```cpp
std::vector<Edge> BuildLeafKnnEdges(const Matrix& points,
                                    const std::vector<int>& leaf,
                                    int k,
                                    bool bidirected,
                                    int scan_cap = 0);
```

This feature should not require changes to `pipnn_builder.cpp`, `runner.cpp`, or CLI parameter routing.

### 5.2 Internal Path Split

Inside `leaf_knn.cpp`, split the work into three internal paths:

1. `naive_full`
- exact current behavior for full scans on small leaves

2. `naive_capped`
- exact current behavior for `scan_cap > 0`
- remains scalar because the capped path is already approximate and the current wave-4 checkpoints do not depend on optimizing it

3. `blocked_full`
- exact full-scan path for larger leaves
- enabled only when:
  - `scan_cap <= 0`
  - leaf size exceeds a threshold such as `64`

This split preserves semantics while avoiding block setup overhead on tiny leaves.

### 5.3 Data Layout

For `blocked_full`:
- gather the leaf's rows from the global point matrix into a compact local matrix `X` of shape `m x d`
- store `X` in row-major layout to make row gathers predictable
- precompute squared norms `norm[i] = ||x_i||^2`
- iterate over `query_block x candidate_block` tiles

Distance reconstruction per tile uses:

```text
||x - y||^2 = ||x||^2 + ||y||^2 - 2 * dot(x, y)
```

Any tiny negative result caused by floating-point error is clamped to `0` before ranking.

### 5.4 Top-k Merge Strategy

For each query row in the current query block:
- maintain a local top-k container across all candidate blocks
- skip self edges
- after each tile, merge only distances that can enter the current top-k frontier
- emit final `(u, v)` edges in the same order-independent semantic role as the current implementation

The merge strategy should optimize for small `k` and moderate leaf size, not for generic sorting workloads.

### 5.5 Dependency Boundary

Use `Eigen` as a header-only dependency integrated through `CMake`.

Requirements:
- the build must fail clearly if `Eigen` is unavailable
- no runtime shared library dependency should be introduced
- `build/`, `build-cov/`, and remote benchmark scripts must continue to build reproducibly after the dependency is added

## 6. Expected File Changes

Primary implementation:
- `src/core/leaf_knn.cpp`
- `src/core/leaf_knn.h`
- `CMakeLists.txt`

Tests:
- `tests/test_leaf_knn.cpp`
- `tests/test_pipnn_integration.cpp`

Potential benchmark scripts if needed for repeatable feature evidence:
- `scripts/bench/run_feature21_leaf_knn_100k_200.sh`

## 7. Error Handling and Fallbacks

- If `Eigen` cannot be configured, fail at build/configure time with a clear error.
- If `leaf.size() <= 1`, return no edges immediately.
- If `k <= 0`, return no edges immediately.
- If `scan_cap > 0`, continue to use the existing capped scalar path.
- If leaf size is below the blocked threshold, continue to use the existing exact scalar full-scan path.

This keeps the optimization bounded to the case where it is most likely to pay off.

## 8. Test Strategy

### 8.1 Unit Coverage

Extend `tests/test_leaf_knn.cpp` to cover:
- empty / singleton leaves
- `k >= leaf_size - 1`
- exact equality between `naive_full` and `blocked_full` on the same leaf
- bidirected edge emission in both paths
- capped path continuing to route through the scalar implementation

### 8.2 Integration Coverage

Extend `tests/test_pipnn_integration.cpp` to prove:
- the builder still produces a valid graph when the blocked path is exercised
- feature-20 `RBC` semantics and feature-19 `HashPrune` diagnostics remain intact
- profile output still contains `partition_sec`, `leaf_knn_sec`, and `prune_sec`

### 8.3 Remote Benchmark Evidence

Run the authoritative remote benchmark on:
- dataset: `SIFT1M`
- slice: `100k/200`
- config: `fanout=2`, `replicas=1`

Evidence to collect:
- `build_sec`
- `recall_at_10`
- `qps`
- `partition_sec`
- `leaf_knn_sec`
- `prune_sec`

## 9. Risks and Mitigations

### Risk 1 — Tile setup cost outweighs compute savings

Mitigation:
- keep a small-leaf fallback threshold
- benchmark only after exact semantic parity is proven locally

### Risk 2 — Top-k merge becomes the new bottleneck

Mitigation:
- keep `k`-aware merge logic simple and local
- avoid global re-sorts of all tile distances

### Risk 3 — Numerical differences perturb recall

Mitigation:
- compare blocked and scalar edge outputs on controlled tests
- clamp tiny negative distances to zero
- enforce the `Recall@10 >= 0.95` stage gate on `100k/200`

## 10. Non-Goals

- no attempt in this feature to optimize the capped-scan approximate path
- no changes to `HashPrune`, `RBC`, beam search, or HNSW baseline behavior
- no `1M/100` authority benchmark in this feature; that remains feature 22

## 11. Approval Boundary

Once approved, the next step is to create the implementation plan for feature 21 and execute it with TDD before touching any other wave-4 feature.
