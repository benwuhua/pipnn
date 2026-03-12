# Multi-Leaf leaf_kNN Batching Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the failed per-leaf exact blocked kernel with a multi-leaf batching path that preserves exact full-scan leaf semantics while reducing repeated matrix setup and scheduling cost on the high-dimensional workload.

**Architecture:** Move the batching boundary up from the single-leaf helper into the builder flow. Keep the scalar single-leaf exact path as the correctness reference, collect uncapped large leaves as jobs, then execute them through a multi-leaf batch helper that packs multiple leaves into one contiguous matrix while keeping exact top-k and no cross-leaf neighbors.

**Tech Stack:** C++20, CMake, existing repo-local `Eigen` dependency, existing `ctest` suite, generic-x86-remote scripts, existing `simplewiki-openai` high-dim smoke wrapper.

---

## File Map

- Modify: `src/core/pipnn_builder.cpp`
  - Move the batching boundary up to the builder: split leaves into immediate scalar work vs deferred exact full-scan jobs, then merge batched edges back into `candidates[u]`.
- Modify: `src/core/leaf_knn.cpp`
  - Keep capped behavior unchanged and keep single-leaf scalar exact behavior available; remove the per-leaf blocked dispatch path.
- Modify: `src/core/leaf_knn_blocked.h`
  - Replace the per-leaf blocked helper surface with multi-leaf batch job/config/planning/execution declarations plus the scalar exact reference helper.
- Modify: `src/core/leaf_knn_blocked.cpp`
  - Implement batch planning, exact multi-leaf execution, and shared row-block helpers; keep the scalar exact implementation as the correctness reference.
- Modify: `tests/test_leaf_knn.cpp`
  - Add deterministic mixed-leaf batch equality tests, no-cross-leaf assertions, and batch-budget planning tests.
- Modify: `tests/test_pipnn_integration.cpp`
  - Keep builder-level graph/regression coverage valid after batching moves into `pipnn_builder.cpp`; add one focused assertion only if existing coverage does not prove the plumbing.
- Modify: `results/high_dim_validation/README.md`
  - Record the post-change high-dim smoke numbers and the next architecture decision.

## Chunk 1: Lock the Multi-Leaf Batch Contract

### Task 1: Add failing tests for mixed-leaf exact batching

**Files:**
- Modify: `tests/test_leaf_knn.cpp`
- Modify: `src/core/leaf_knn_blocked.h`
- Modify: `src/core/leaf_knn_blocked.cpp`

- [ ] **Step 1: Write the failing tests**

Extend `tests/test_leaf_knn.cpp` so it targets the future multi-leaf helper surface directly.

Cover these behaviors:

- exact batched output for two leaves equals the concatenation of scalar exact per-leaf outputs
- no emitted edge crosses leaf boundaries
- `k >= leaf_size - 1` still emits all valid neighbors inside each leaf
- capped path remains separate and is not folded into the new batch helper

Use a helper surface shaped like:

```cpp
namespace pipnn {
struct LeafBatchJob {
  std::vector<int> leaf;
};

struct LeafBatchConfig {
  int min_leaf_for_batch = 64;
  int point_block_rows = 128;
  int max_points_per_batch = 1024;
};

std::vector<Edge> BuildLeafKnnExactEdgesNaive(const Matrix& points,
                                              const std::vector<int>& leaf,
                                              int k,
                                              bool bidirected);

std::vector<Edge> BuildLeafKnnExactBatchedEdges(const Matrix& points,
                                                const std::vector<LeafBatchJob>& jobs,
                                                int k,
                                                bool bidirected,
                                                const LeafBatchConfig& cfg = {});
}  // namespace pipnn
```

- [ ] **Step 2: Run the targeted test to verify red**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: build or test failure because the multi-leaf helper API does not exist yet.

- [ ] **Step 3: Add the helper declarations and a scalar placeholder executor**

In `src/core/leaf_knn_blocked.h/.cpp`:

- add `LeafBatchJob` and `LeafBatchConfig`
- expose a scalar exact helper for one leaf
- implement `BuildLeafKnnExactBatchedEdges(...)` as a temporary scalar concatenation over all jobs

This first green step must preserve exactness, even if it is not faster yet.

- [ ] **Step 4: Re-run the targeted test to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: `leaf_knn` passes with the scalar placeholder batch executor.

- [ ] **Step 5: Commit**

```bash
git add tests/test_leaf_knn.cpp src/core/leaf_knn_blocked.h src/core/leaf_knn_blocked.cpp
git commit -m "test: lock multi-leaf knn batch semantics"
```

## Chunk 2: Implement Batch Planning and Exact Multi-Leaf Execution

### Task 2: Add batch planning and the real multi-leaf executor

**Files:**
- Modify: `src/core/leaf_knn_blocked.h`
- Modify: `src/core/leaf_knn_blocked.cpp`
- Modify: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Add failing tests for batch planning**

Extend `tests/test_leaf_knn.cpp` with deterministic planning checks for a future helper such as:

```cpp
namespace pipnn {
struct LeafBatchPlan {
  std::vector<int> job_indices;
  int total_points = 0;
};

std::vector<LeafBatchPlan> PlanLeafKnnBatches(const std::vector<LeafBatchJob>& jobs,
                                              const LeafBatchConfig& cfg = {});
}  // namespace pipnn
```

Assertions should cover:

- similar-sized leaves can land in the same plan batch
- no `LeafBatchPlan.total_points` exceeds `max_points_per_batch`
- low `max_points_per_batch` forces multiple batches

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: failure because planning helpers do not exist yet.

- [ ] **Step 2: Implement batch planning**

In `src/core/leaf_knn_blocked.cpp`:

- add a small size-bucket classifier
- group only jobs whose leaf size is `>= min_leaf_for_batch`
- accumulate jobs into `LeafBatchPlan` instances until `max_points_per_batch` would be exceeded
- leave smaller jobs to the scalar path

Keep this logic deterministic and local to the file. Do not add CLI flags in this iteration.

- [ ] **Step 3: Replace the placeholder executor with exact multi-leaf batching**

Still in `src/core/leaf_knn_blocked.cpp`:

- materialize one row-major matrix per planned batch
- keep per-leaf `(offset, size, global ids)` metadata
- precompute row norms for the whole batch
- process row blocks with `Eigen`
- for each query row, restrict candidates to the current leaf subrange only
- keep exact top-k, self-edge exclusion, and bidirected behavior

Do not emit any cross-leaf edge.

- [ ] **Step 4: Re-run the focused leaf tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: mixed-leaf equality tests and planning tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_knn_blocked.h src/core/leaf_knn_blocked.cpp tests/test_leaf_knn.cpp
git commit -m "feat: add multi-leaf knn batch executor"
```

## Chunk 3: Move the Batching Boundary Into the Builder

### Task 3: Route large uncapped leaves through the batch executor

**Files:**
- Modify: `src/core/pipnn_builder.cpp`
- Modify: `src/core/leaf_knn.cpp`
- Modify: `tests/test_pipnn_integration.cpp`
- Test: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Add one failing integration assertion only if current coverage is insufficient**

Review `tests/test_pipnn_integration.cpp`.

If the current assertions do not adequately cover builder-level candidate generation after batching moves into `pipnn_builder.cpp`, add one focused check using a deterministic workload where at least one leaf is large enough to batch. Good options:

- compare `candidate_edges` against a scalar reference built from `BuildRbcLeaves(...)` plus per-leaf `BuildLeafKnnEdges(...)`
- assert graph edge count and prune counters remain sane after the new plumbing

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(pipnn_integration|leaf_knn)$'
```

Expected: red only if a new assertion was added.

- [ ] **Step 2: Remove the per-leaf blocked dispatch from `leaf_knn.cpp`**

Refactor `src/core/leaf_knn.cpp` so:

- capped behavior stays unchanged
- uncapped single-leaf behavior stays scalar exact
- the file no longer chooses a per-leaf blocked mode

`BuildLeafKnnEdges(...)` should remain the stable single-leaf path used for:

- capped leaves
- small uncapped leaves
- scalar reference calculations in tests

- [ ] **Step 3: Add deferred leaf collection and batched execution to `pipnn_builder.cpp`**

In `BuildPipnnGraph(...)`:

- keep the existing leaf traversal order
- for each leaf:
  - if `scan_cap > 0`, build edges immediately through the existing single-leaf path
  - if the leaf is too small for batching, build edges immediately through the existing single-leaf exact path
  - otherwise append a `LeafBatchJob` to a deferred list
- after the traversal for that replica, execute the deferred jobs through `BuildLeafKnnExactBatchedEdges(...)`
- merge the returned edges into `candidates[u]` exactly as today

Do not change `HashPrune`, `RBC`, or search code in this task.

- [ ] **Step 4: Run the affected local tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(leaf_knn|pipnn_integration)$'
```

Expected: both tests pass.

- [ ] **Step 5: Run the full local suite**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/core/pipnn_builder.cpp src/core/leaf_knn.cpp tests/test_pipnn_integration.cpp tests/test_leaf_knn.cpp
git commit -m "refactor: batch leaf knn work at builder level"
```

## Chunk 4: High-Dimensional Authority Smoke and Decision

### Task 4: Re-run the high-dim smoke and record whether the new path is promotion-ready

**Files:**
- Modify: `results/high_dim_validation/README.md`
- Artifact: `results/high_dim_validation/simplewiki_openai_5k_50/*`
- Artifact: `remote-logs/high_dim_validation/*` (local evidence only if tracked by project policy)

- [ ] **Step 1: Validate the wrapper syntax locally**

Run:

```bash
bash -n scripts/bench/run_simplewiki_openai_20k_100.sh
```

Expected: no syntax errors.

- [ ] **Step 2: Sync the repo to the remote smoke location**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
REMOTE_REPO=/data/work/pipnn-high-dim-validation-repo
"${REMOTE_TOOLS_DIR}/sync.sh" --src . --dest "${REMOTE_REPO}" --delete \
  --exclude .git --exclude .worktrees --exclude build --exclude 'build-*' \
  --exclude results --exclude remote-logs
```

Expected: `sync=ok`.

- [ ] **Step 3: Run the authoritative high-dim smoke**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
REMOTE_REPO=/data/work/pipnn-high-dim-validation-repo
"${REMOTE_TOOLS_DIR}/run-bg.sh" --repo "${REMOTE_REPO}" --name simplewiki-openai-5k50-multi-leaf-batch \
  -- env MAX_BASE=5000 MAX_QUERY=50 OUT_DIR=results/simplewiki_openai_5k_50 \
     bash scripts/bench/run_simplewiki_openai_20k_100.sh
```

Expected: background job starts and emits a log path.

- [ ] **Step 4: Fetch the refreshed artifacts back**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
mkdir -p results/high_dim_validation remote-logs/high_dim_validation
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src /data/work/pipnn-high-dim-validation-repo/results/simplewiki_openai_5k_50 \
  --dest results/high_dim_validation/simplewiki_openai_5k_50
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src /data/work/logs \
  --dest remote-logs/high_dim_validation
```

Expected: refreshed `pipnn_metrics.json`, `pipnn.stdout`, `hnsw_metrics.json`, and the `simplewiki-openai-5k50-multi-leaf-batch_*.log` file are available locally.

- [ ] **Step 5: Update the validation README**

Update `results/high_dim_validation/README.md` with:

- post-RBC baseline numbers
- failed per-leaf blocked numbers
- new multi-leaf batching numbers
- whether the new path is only informative or truly promotion-ready
- next decision:
  - continue leaf batching refinement
  - or stop exact leaf batching and move to a broader shared distance layer / different candidate strategy

- [ ] **Step 6: Commit**

```bash
git add results/high_dim_validation/README.md results/high_dim_validation/simplewiki_openai_5k_50
git commit -m "chore: refresh high-dim multi-leaf batching evidence"
```
