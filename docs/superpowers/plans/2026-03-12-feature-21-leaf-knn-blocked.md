# Feature 21 Blocked leaf_kNN Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current per-point full-scan `leaf_kNN` path with an Eigen-backed blocked kernel that preserves full-scan edge semantics while reducing `leaf_knn_sec` and total build time on the wave-4 `100k/200` benchmark.

**Architecture:** Keep `BuildLeafKnnEdges(...)` as the only public entry point. Internally split the implementation into `naive_full`, `naive_capped`, and `blocked_full`, with a small-leaf threshold routing tiny leaves to the existing scalar path and larger full scans to the blocked Eigen path. Use targeted tests to prove exact edge parity between scalar and blocked full-scan modes before running remote authority benchmarks.

**Tech Stack:** C++20, CMake, OpenMP, Eigen (header-only), ctest, remote x86 benchmark scripts.

---

## File Map

- Modify: `CMakeLists.txt`
  - Add the Eigen dependency in a reproducible way for the worktree build.
- Modify: `src/core/leaf_knn.h`
  - Add the minimum internal test hook needed to force the blocked path without changing the public builder contract.
- Modify: `src/core/leaf_knn.cpp`
  - Split the current implementation into scalar and blocked internal helpers.
  - Implement blocked full-scan distance tiles and top-k merging.
- Modify: `tests/test_leaf_knn.cpp`
  - Add exact parity tests for scalar vs blocked paths and edge-case coverage.
- Modify: `tests/test_pipnn_integration.cpp`
  - Ensure the blocked path can be exercised inside the full builder pipeline without disturbing feature-19/20 diagnostics.
- Create: `scripts/bench/run_feature21_leaf_knn_100k_200.sh`
  - Remote benchmark wrapper for repeatable evidence at `100k/200`, `fanout=2`, `replicas=1`.
- Modify: `task-progress.md`
  - Record local verification and remote benchmark evidence.
- Modify: `RELEASE_NOTES.md`
  - Record the feature-21 optimization checkpoint.

## Chunk 1: Test Hooks And Dependency Wiring

### Task 1: Add a minimal internal path-selection hook

**Files:**
- Modify: `src/core/leaf_knn.h`
- Modify: `src/core/leaf_knn.cpp`
- Test: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Write the failing test for forced blocked-path parity**

Add a new test section to `tests/test_leaf_knn.cpp` that:
- builds a leaf with at least `8` points in `2D`
- calls the current exact full-scan path
- calls a new internal helper or options-based overload that forces the blocked full path on the same leaf
- asserts identical edge pairs for `k=1` and `k=2`

Expected code shape:

```cpp
pipnn::LeafKnnDebugOptions opts;
opts.force_blocked_full = true;
auto exact = pipnn::BuildLeafKnnEdges(points, leaf, 2, false);
auto blocked = pipnn::BuildLeafKnnEdgesDebug(points, leaf, 2, false, 0, opts);
assert(exact == blocked);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: `FAIL` because the debug hook or blocked implementation does not exist yet.

- [ ] **Step 3: Add the smallest internal API needed for testing**

Implement in `src/core/leaf_knn.h/.cpp`:
- a small internal options struct such as `LeafKnnDebugOptions`
- a debug/test-only entry point such as `BuildLeafKnnEdgesDebug(...)`
- default production behavior that still routes `BuildLeafKnnEdges(...)` exactly as before

Keep the existing public `BuildLeafKnnEdges(...)` signature unchanged.

- [ ] **Step 4: Re-run the targeted test to verify the hook compiles**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^leaf_knn$'
```

Expected: the new test still fails, but now because blocked full-scan logic is not implemented, not because symbols are missing.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_knn.h src/core/leaf_knn.cpp tests/test_leaf_knn.cpp
git commit -m "test: add leaf-knn blocked path hook"
```

### Task 2: Wire Eigen into the build

**Files:**
- Modify: `CMakeLists.txt`
- Test: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Add a failing compile step for Eigen-backed code**

Introduce a minimal include in `src/core/leaf_knn.cpp`:

```cpp
#include <Eigen/Dense>
```

without yet changing logic.

- [ ] **Step 2: Build to confirm dependency wiring is missing**

Run:

```bash
cmake --build build -j
```

Expected: compile failure if Eigen is not yet wired through CMake.

- [ ] **Step 3: Integrate Eigen in `CMakeLists.txt`**

Add a header-only dependency path using one reproducible mechanism:
- preferred: `FetchContent` pinned to a stable Eigen release
- alternative: `find_package(Eigen3 CONFIG REQUIRED)` only if the existing environment already guarantees it

Then link/include it for `pipnn_core`.

- [ ] **Step 4: Rebuild to verify dependency resolution**

Run:

```bash
cmake -S . -B build
cmake --build build -j
```

Expected: successful build with no logic changes yet.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/core/leaf_knn.cpp
git commit -m "build: add eigen dependency for blocked leaf-knn"
```

## Chunk 2: Blocked Full-Scan Kernel

### Task 3: Implement blocked full-scan distance tiles with exact edge parity

**Files:**
- Modify: `src/core/leaf_knn.cpp`
- Modify: `src/core/leaf_knn.h`
- Test: `tests/test_leaf_knn.cpp`

- [ ] **Step 1: Expand `tests/test_leaf_knn.cpp` with failing parity coverage**

Add tests for:
- empty and singleton leaves return no edges
- `k >= leaf_size - 1`
- exact equality between scalar full-scan and forced blocked full-scan on the same leaf
- bidirected parity between scalar and blocked full-scan
- capped path still follows the scalar route

Expected failing checks:
- blocked path returns different edge order/content
- blocked path not yet implemented

- [ ] **Step 2: Run the targeted test suite to verify failure**

Run:

```bash
ctest --test-dir build --output-on-failure -R '^(leaf_knn|distance)$'
```

Expected: `leaf_knn` fails on new parity assertions.

- [ ] **Step 3: Implement the blocked full-scan kernel**

In `src/core/leaf_knn.cpp`:
- factor the current code into helpers:
  - `BuildLeafKnnEdgesNaiveFull`
  - `BuildLeafKnnEdgesNaiveCapped`
  - `BuildLeafKnnEdgesBlockedFull`
- gather the leaf into a compact row-major matrix
- precompute row norms
- compute tiled dot products with Eigen
- reconstruct squared distances as `norm_i + norm_j - 2 * dot`
- clamp tiny negative results to `0`
- maintain per-row top-k state across candidate blocks
- preserve exact edge semantics for full scans
- route to blocked full only when `scan_cap <= 0` and `leaf.size()` exceeds a fixed threshold such as `64`

- [ ] **Step 4: Re-run targeted tests to verify parity**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(leaf_knn|distance)$'
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_knn.h src/core/leaf_knn.cpp tests/test_leaf_knn.cpp
git commit -m "feat: add blocked full-scan leaf-knn kernel"
```

### Task 4: Prove integration compatibility inside the builder pipeline

**Files:**
- Modify: `tests/test_pipnn_integration.cpp`
- Modify: `src/core/leaf_knn.cpp` (only if a routing or threshold bug is exposed)

- [ ] **Step 1: Add a failing integration test that exercises the blocked path**

Extend `tests/test_pipnn_integration.cpp` with a case that:
- constructs enough points to exceed the blocked threshold within a leaf
- runs the builder with feature-20-style params
- asserts a non-empty graph and stable diagnostics
- confirms the build path still works with `PIPNN_PROFILE` enabled or equivalent stats collection

- [ ] **Step 2: Run the targeted integration test and verify failure if routing is wrong**

Run:

```bash
ctest --test-dir build --output-on-failure -R '^pipnn_integration$'
```

Expected: failure only if blocked-path routing or builder compatibility is incomplete.

- [ ] **Step 3: Make the minimal integration fix**

If needed:
- adjust the threshold, routing, or helper visibility
- do not change `HashPrune`, `RBC`, or runner semantics in this task

- [ ] **Step 4: Re-run the local integration slice**

Run:

```bash
ctest --test-dir build --output-on-failure -R '^(leaf_knn|pipnn_integration|runner_metrics)$'
ctest --test-dir build --output-on-failure
```

Expected: all targeted tests pass, then the full local suite passes.

- [ ] **Step 5: Commit**

```bash
git add tests/test_pipnn_integration.cpp src/core/leaf_knn.cpp
git commit -m "test: verify blocked leaf-knn builder integration"
```

## Chunk 3: Remote Evidence And Feature Checkpoint

### Task 5: Add a repeatable remote benchmark wrapper

**Files:**
- Create: `scripts/bench/run_feature21_leaf_knn_100k_200.sh`

- [ ] **Step 1: Write the wrapper script**

Mirror the style of the existing feature scripts and make it run:
- dataset: `SIFT1M`
- slice: `100k/200`
- params: feature-20 checkpoint settings (`fanout=2`, `replicas=1`)
- output directory: `results/feature21_f2_r1_100k_200_v3/` or the next free suffix

The script should fetch/build/run in the remote x86 environment and sync results back.

- [ ] **Step 2: Dry-run the script locally for syntax errors**

Run:

```bash
bash -n scripts/bench/run_feature21_leaf_knn_100k_200.sh
```

Expected: no shell syntax errors.

- [ ] **Step 3: Commit**

```bash
git add scripts/bench/run_feature21_leaf_knn_100k_200.sh
git commit -m "bench: add feature 21 remote benchmark wrapper"
```

### Task 6: Run authority benchmark and update project evidence

**Files:**
- Modify: `task-progress.md`
- Modify: `RELEASE_NOTES.md`
- Create or update: `results/feature21_f2_r1_100k_200_v3/`

- [ ] **Step 1: Run the remote benchmark**

Run:

```bash
bash scripts/bench/run_feature21_leaf_knn_100k_200.sh
```

Expected evidence files:
- `results/feature21_f2_r1_100k_200_v3/pipnn_metrics.json`
- `results/feature21_f2_r1_100k_200_v3/pipnn.stdout`

- [ ] **Step 2: Evaluate the stage gate**

Check that the new result satisfies:
- `recall_at_10 >= 0.95`
- `leaf_knn_sec < 30.8155`
- `build_sec < 68.4892`

If it does not, stop and record the failed attempt instead of marking feature 21 complete.

- [ ] **Step 3: Update progress docs**

Record:
- the new metrics
- comparison against the feature-20 checkpoint
- whether the blocked kernel is a retained optimization or a rejected experiment

- [ ] **Step 4: Run final local verification before claiming success**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: local suite passes.

- [ ] **Step 5: Commit**

```bash
git add task-progress.md RELEASE_NOTES.md results/feature21_f2_r1_100k_200_v3
# plus any remaining code/test files if benchmark-driven tweaks were required
git commit -m "feat: checkpoint feature 21 blocked leaf-knn"
```
