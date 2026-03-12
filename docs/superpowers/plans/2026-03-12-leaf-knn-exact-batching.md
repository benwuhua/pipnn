# Exact leaf_kNN Batching Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce `leaf_knn_sec` with an exact blocked full-scan leaf kernel while preserving the current post-RBC-batching graph semantics and high-dimensional recall.

**Architecture:** Split leaf-internal exact full-scan logic into a focused helper so naive and blocked paths can be tested directly against each other. Keep capped-scan behavior unchanged, keep the public `BuildLeafKnnEdges(...)` interface stable, and only route uncapped large leaves into the new `Eigen` blocked full-scan path. Validate locally with direct edge-equality tests, then remotely on the existing `simplewiki-openai 5k/50` smoke workload.

**Tech Stack:** C++20, CMake, existing repo-local `Eigen` dependency, existing `ctest` suite, generic-x86-remote scripts, existing high-dim smoke wrapper.

---

## File Map

- Modify: `src/core/leaf_knn.cpp`
  - Keep the public entry point and capped behavior stable; dispatch between naive full-scan, capped scan, and blocked full-scan.
- Create: `src/core/leaf_knn_blocked.h`
  - Declare small internal helper APIs for exact naive and exact blocked full-scan leaf edge generation plus dispatch config.
- Create: `src/core/leaf_knn_blocked.cpp`
  - Implement exact naive full-scan, blocked full-scan, and mode selection.
- Modify: `CMakeLists.txt`
  - Add the new helper source file to `pipnn_core`.
- Modify: `tests/test_leaf_knn.cpp`
  - Add deterministic direct comparisons between naive and blocked full-scan paths, dispatch-threshold checks, and regression coverage for capped behavior.
- Modify: `results/high_dim_validation/README.md`
  - Record the post-leaf-batching remote numbers and the next algorithm decision.

## Chunk 1: Lock Exact leaf_kNN Semantics Behind a Helper

### Task 1: Add failing tests for exact naive vs blocked equality

**Files:**
- Modify: `tests/test_leaf_knn.cpp`
- Create: `src/core/leaf_knn_blocked.h`
- Create: `src/core/leaf_knn_blocked.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing tests**

Extend `tests/test_leaf_knn.cpp` so it targets future helper functions directly.

Cover these behaviors:

- exact naive full-scan and exact blocked full-scan return identical edges on a deterministic fixed leaf
- self edges are never emitted
- `k >= leaf_size - 1` still emits all valid neighbors without duplicates from self edges
- capped path remains a separate behavior and is not folded into the exact comparison tests

Suggested helper surface to target:

```cpp
namespace pipnn {
enum class LeafKnnMode { NaiveFull, BlockedFull };

struct LeafKnnConfig {
  int min_leaf_for_blocked = 128;
  int point_block_rows = 128;
};

std::vector<Edge> BuildLeafKnnExactEdges(const Matrix& points,
                                         const std::vector<int>& leaf,
                                         int k,
                                         bool bidirected,
                                         LeafKnnMode mode,
                                         const LeafKnnConfig& cfg = {});
}  // namespace pipnn
```

- [ ] **Step 2: Run the targeted test to verify red**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: build or test failure because the helper API does not exist yet.

- [ ] **Step 3: Add the helper interface and minimal naive implementation**

Create `src/core/leaf_knn_blocked.h/.cpp` and add it to `pipnn_core` in `CMakeLists.txt`.

For the first green step:

- implement exact naive full-scan helper matching today's uncapped semantics
- let `BlockedFull` temporarily delegate to the naive implementation
- keep the public `BuildLeafKnnEdges(...)` unchanged for now

- [ ] **Step 4: Re-run the targeted test to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: `leaf_knn` passes with the placeholder blocked path.

- [ ] **Step 5: Commit**

```bash
git add tests/test_leaf_knn.cpp src/core/leaf_knn_blocked.h src/core/leaf_knn_blocked.cpp CMakeLists.txt
git commit -m "test: lock exact leaf knn semantics"
```

## Chunk 2: Add the Exact Blocked Full-Scan Kernel

### Task 2: Implement blocked full-scan and dispatch thresholds

**Files:**
- Modify: `src/core/leaf_knn_blocked.h`
- Modify: `src/core/leaf_knn_blocked.cpp`
- Modify: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Add a failing dispatch-threshold test**

Extend `tests/test_leaf_knn.cpp` with a small mode-selection check for a future helper such as:

```cpp
LeafKnnMode SelectLeafKnnMode(std::size_t leaf_size,
                              int scan_cap,
                              const LeafKnnConfig& cfg = {});
```

Assertions should cover:

- small uncapped leaf -> `NaiveFull`
- large uncapped leaf -> `BlockedFull`
- capped leaf -> `NaiveFull`

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: failure because mode selection does not exist yet.

- [ ] **Step 2: Implement the blocked full-scan kernel**

In `src/core/leaf_knn_blocked.cpp`:

- materialize a row-major matrix for the leaf vectors
- precompute row norms
- process query rows in blocks
- compute `X_block * X^T`
- recover squared distances
- skip diagonal entries
- preserve exact row-level top-k selection
- preserve bidirected edge emission

This implementation must remain exact relative to the same uncapped candidate set.

- [ ] **Step 3: Add mode selection helper**

Implement a small dispatch helper that returns:

- `NaiveFull` for small uncapped leaves
- `NaiveFull` for any capped leaf
- `BlockedFull` for large uncapped leaves

Thresholds stay internal to the helper in this iteration.

- [ ] **Step 4: Re-run focused leaf tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: direct naive-vs-blocked equality and threshold tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_knn_blocked.h src/core/leaf_knn_blocked.cpp tests/test_leaf_knn.cpp
git commit -m "feat: add exact blocked leaf knn kernel"
```

## Chunk 3: Reconnect the Public leaf_kNN Entry Point

### Task 3: Route `BuildLeafKnnEdges(...)` through the helper

**Files:**
- Modify: `src/core/leaf_knn.cpp`
- Test: `tests/test_leaf_knn.cpp`
- Test: `tests/test_pipnn_integration.cpp`

- [ ] **Step 1: Add an integration assertion only if needed**

If current tests do not sufficiently exercise public `BuildLeafKnnEdges(...)` after the refactor, add one small assertion in `tests/test_pipnn_integration.cpp` that ensures candidate edges still contribute to the same graph invariants.

If current coverage is already enough, skip this step and note why in the commit message or progress notes.

- [ ] **Step 2: Replace the uncapped scalar path in `leaf_knn.cpp`**

Refactor `BuildLeafKnnEdges(...)` so:

- capped path continues to use the existing capped behavior
- small uncapped leaves continue to use exact naive full-scan
- large uncapped leaves route to blocked full-scan via the helper

Do not change the public signature.

- [ ] **Step 3: Run affected local tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(leaf_knn|pipnn_integration)$'
```

Expected: both tests pass.

- [ ] **Step 4: Run the full local suite**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_knn.cpp tests/test_pipnn_integration.cpp tests/test_leaf_knn.cpp
git commit -m "refactor: route leaf knn through exact batching helper"
```

## Chunk 4: High-Dimensional Authority Smoke

### Task 4: Re-run the existing high-dim smoke and record the decision

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
"${REMOTE_TOOLS_DIR}/run-bg.sh" --repo "${REMOTE_REPO}" --name simplewiki-openai-5k50-leaf-batch \
  -- env MAX_BASE=5000 MAX_QUERY=50 OUT_DIR=results/simplewiki_openai_5k_50 \
     bash scripts/bench/run_simplewiki_openai_20k_100.sh
```

Expected: background job starts and emits a log path.

- [ ] **Step 4: Fetch the updated artifacts back**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
mkdir -p results/high_dim_validation remote-logs/high_dim_validation
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src /data/work/pipnn-high-dim-validation-repo/results/simplewiki_openai_5k_50 \
  --dest results/high_dim_validation/simplewiki_openai_5k_50
"${REMOTE_TOOLS_DIR}/run.sh" --workdir /data/work/logs \
  -- cat simplewiki-openai-5k50-leaf-batch_*.log
```

Expected: refreshed `pipnn_metrics.json`, `pipnn.stdout`, `hnsw_metrics.json` are available locally and the remote log is accessible.

- [ ] **Step 5: Update the validation README**

Add a new section that compares the post-leaf-batching numbers against the post-RBC-batching checkpoint:

- old `build_sec`, `leaf_knn_sec`, `partition_sec`, `prune_sec`
- new `build_sec`, `leaf_knn_sec`, `partition_sec`, `prune_sec`
- whether recall changed
- next algorithm decision

- [ ] **Step 6: Commit**

```bash
git add results/high_dim_validation/README.md results/high_dim_validation/simplewiki_openai_5k_50
git commit -m "chore: refresh high-dim leaf batching evidence"
```
