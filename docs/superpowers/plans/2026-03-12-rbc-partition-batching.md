# RBC Partition Batching Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce the dominant `RBC partition` cost by batching point-to-leader distance computation with `Eigen`, while preserving the current `Feature 20` tree behavior and high-dimensional recall profile.

**Architecture:** Split leader assignment out of `rbc.cpp` into a small internal helper so scalar and blocked paths can be tested directly against each other. Keep `BuildRbcLeaves(...)` semantics unchanged and only swap the internal assignment kernel that maps node points to sampled leaders. Validate the change locally with deterministic equality tests, then remotely on the existing `simplewiki-openai 5k/50` smoke workload.

**Tech Stack:** C++20, CMake, FetchContent, Eigen, existing `ctest` suite, generic-x86-remote scripts, existing high-dim smoke wrapper.

---

## File Map

- Modify: `CMakeLists.txt`
  - Add `Eigen` as a reproducible dependency and link it into `pipnn_core`.
- Create: `src/core/rbc_assignment.h`
  - Declare a focused internal API for scalar and blocked point-to-leader assignment plus the small config/diagnostic types it needs.
- Create: `src/core/rbc_assignment.cpp`
  - Implement the scalar reference path first, then the blocked `Eigen` path, and keep the tie-break semantics explicit.
- Modify: `src/core/rbc.cpp`
  - Replace the inlined scalar leader-assignment loop with calls into the new helper; keep recursive tree construction and leaf overlap unchanged.
- Modify: `tests/test_rbc.cpp`
  - Add deterministic assignment-equivalence tests, equal-distance tie tests, and threshold-boundary checks.
- Modify: `scripts/bench/run_simplewiki_openai_20k_100.sh`
  - Keep the wrapper reusable for this iteration and allow fast reruns of `5k/50` and optional larger follow-up slices without changing the script body.
- Modify: `results/high_dim_validation/README.md`
  - Record the new post-change numbers and the algorithm decision.

## Chunk 1: Extract and Lock Scalar Assignment Semantics

### Task 1: Add failing tests for assignment equivalence and tie behavior

**Files:**
- Modify: `tests/test_rbc.cpp`
- Create: `src/core/rbc_assignment.h`
- Create: `src/core/rbc_assignment.cpp`

- [ ] **Step 1: Write the failing assignment tests**

Add tests that target the future helper directly. Cover these behaviors:

- scalar and blocked assignment produce identical bucket indices on a fixed synthetic node
- equal-distance inputs preserve the current tie-break behavior
- threshold-boundary inputs can force both scalar and blocked modes and still agree

Use deterministic fixtures with explicit expected outputs. Example shape to target:

```cpp
pipnn::Matrix points = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {2.0f, 0.0f},
    {3.0f, 0.0f},
};
std::vector<int> ids = {0, 1, 2, 3};
std::vector<int> leaders = {0, 3};
```

The test should call a helper with a clear mode selector such as `Scalar` vs `Blocked` and compare the resulting assignment vector.

- [ ] **Step 2: Run the targeted test to verify red**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: build or test failure because the helper API does not exist yet.

- [ ] **Step 3: Create the helper interface and scalar reference implementation**

Create a small helper surface in `src/core/rbc_assignment.h/.cpp` with:

- an assignment mode enum
- a config struct with size thresholds and block size
- a result type that returns one nearest-leader index per point in `ids`
- a scalar implementation that exactly matches the current `rbc.cpp` semantics

Minimal interface target:

```cpp
namespace pipnn {
enum class RbcAssignMode { Scalar, Blocked };

struct RbcAssignConfig {
  int min_points_for_blocked = 256;
  int min_leaders_for_blocked = 8;
  int point_block_rows = 256;
};

std::vector<int> AssignPointsToLeaders(const Matrix& points,
                                       const std::vector<int>& ids,
                                       const std::vector<int>& leaders,
                                       RbcAssignMode mode,
                                       const RbcAssignConfig& cfg = {});
}  // namespace pipnn
```

For this first green step, the `Blocked` mode may temporarily delegate to the scalar implementation so the tests compile and run.

- [ ] **Step 4: Re-run the targeted test to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: `rbc` passes with the scalar placeholder implementation.

- [ ] **Step 5: Commit**

```bash
git add tests/test_rbc.cpp src/core/rbc_assignment.h src/core/rbc_assignment.cpp
git commit -m "test: lock rbc assignment semantics"
```

## Chunk 2: Add the Blocked Eigen Assignment Kernel

### Task 2: Wire `Eigen` and implement blocked assignment

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/core/rbc_assignment.cpp`
- Modify: `tests/test_rbc.cpp`

- [ ] **Step 1: Add reproducible `Eigen` dependency support**

Update `CMakeLists.txt` to fetch `Eigen` reproducibly. Keep it parallel to the existing `hnswlib` setup.

Target shape:

```cmake
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
)
FetchContent_MakeAvailable(eigen)

target_link_libraries(pipnn_core PUBLIC Eigen3::Eigen)
```

If the exact imported target name differs, adjust based on the fetched package, but keep the dependency repo-local and deterministic.

- [ ] **Step 2: Add a failing blocked-path-specific test**

Extend `tests/test_rbc.cpp` with at least one case large enough to trigger the future blocked path based on the configured thresholds. The test should still assert exact equality against the scalar result.

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: failure or placeholder mismatch because blocked mode is not implemented yet.

- [ ] **Step 3: Implement the blocked assignment kernel**

In `src/core/rbc_assignment.cpp`:

- materialize row-major point and leader matrices
- compute row norms for points and leaders
- process points in blocks
- compute `X * L^T`
- recover squared distances with the norm identity
- select the nearest leader per point
- preserve the current tie-break explicitly

Keep the scalar implementation in the same file as the reference path.

- [ ] **Step 4: Add internal dispatch helper**

Add a small helper that chooses `Scalar` vs `Blocked` based on:

- point count threshold
- leader count threshold

Do not expose this dispatch policy through the CLI in this iteration.

- [ ] **Step 5: Re-run the focused RBC tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: `rbc` passes and blocked/scalar equality holds.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/core/rbc_assignment.cpp tests/test_rbc.cpp
git commit -m "feat: add blocked rbc assignment kernel"
```

## Chunk 3: Reconnect `rbc.cpp` Without Changing Tree Semantics

### Task 3: Route `BuildNode(...)` through the assignment helper

**Files:**
- Modify: `src/core/rbc.cpp`
- Test: `tests/test_rbc.cpp`
- Test: `tests/test_pipnn_integration.cpp`

- [ ] **Step 1: Add a failing integration assertion if needed**

If current tests do not cover stable end-to-end `RBC` behavior strongly enough, add one integration assertion that checks leaf stats remain sane after the helper is wired in. Keep it small and deterministic.

Good examples:

- `assignment_total` remains bounded by `fanout`
- `max_membership` remains bounded
- no empty leaves are emitted

- [ ] **Step 2: Replace the inlined scalar loop in `rbc.cpp`**

Refactor `BuildNode(...)` so leader assignment is delegated to the helper.

Requirements:

- keep leader sampling unchanged
- keep bucket construction unchanged
- keep recursion and chunk-split behavior unchanged
- keep overlap application unchanged

Only the nearest-leader computation should move.

- [ ] **Step 3: Run the affected local tests**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(rbc|pipnn_integration)$'
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
git add src/core/rbc.cpp tests/test_pipnn_integration.cpp
git commit -m "refactor: route rbc build through assignment helper"
```

## Chunk 4: High-Dimension Authority Smoke

### Task 4: Re-run the existing high-dim smoke and record the decision

**Files:**
- Modify: `scripts/bench/run_simplewiki_openai_20k_100.sh`
- Modify: `results/high_dim_validation/README.md`
- Artifact: `results/high_dim_validation/simplewiki_openai_5k_50/*`

- [ ] **Step 1: Make the wrapper convenient for repeat runs**

If needed, lightly adjust `run_simplewiki_openai_20k_100.sh` so `MAX_BASE`, `MAX_QUERY`, and `OUT_DIR` overrides are documented and stable. Do not change the core command shape.

- [ ] **Step 2: Validate script syntax locally**

Run:

```bash
bash -n scripts/bench/run_simplewiki_openai_20k_100.sh
```

Expected: no syntax errors.

- [ ] **Step 3: Sync the repo to the remote smoke location**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
REMOTE_REPO=/data/work/pipnn-high-dim-validation-repo
"${REMOTE_TOOLS_DIR}/sync.sh" --src . --dest "${REMOTE_REPO}" --delete \
  --exclude .git --exclude .worktrees --exclude build --exclude 'build-*' \
  --exclude results --exclude remote-logs
```

Expected: `sync=ok`.

- [ ] **Step 4: Run the authoritative high-dim smoke on the remote host**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
REMOTE_REPO=/data/work/pipnn-high-dim-validation-repo
"${REMOTE_TOOLS_DIR}/run-bg.sh" --repo "${REMOTE_REPO}" --name simplewiki-openai-5k50 \
  -- env MAX_BASE=5000 MAX_QUERY=50 OUT_DIR=results/simplewiki_openai_5k_50 \
     bash scripts/bench/run_simplewiki_openai_20k_100.sh
```

Expected: background job starts and emits a log path.

- [ ] **Step 5: Fetch the artifacts back**

Run:

```bash
REMOTE_TOOLS_DIR=/Users/ryan/.codex/skills/generic-x86-remote/scripts
mkdir -p results/high_dim_validation remote-logs/high_dim_validation
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src /data/work/pipnn-high-dim-validation-repo/results/simplewiki_openai_5k_50 \
  --dest results/high_dim_validation/simplewiki_openai_5k_50
"${REMOTE_TOOLS_DIR}/fetch.sh" --src /data/work/logs --dest remote-logs/high_dim_validation
```

Expected: updated `pipnn_metrics.json`, `pipnn.stdout`, `hnsw_metrics.json` are available locally.

- [ ] **Step 6: Record the decision in the validation README**

Update `results/high_dim_validation/README.md` with:

- old `partition_sec` / `build_sec`
- new `partition_sec` / `build_sec`
- whether the change was enough to keep `RBC`-first iteration justified
- the next algorithm decision (`continue into leaf_kNN batching` vs `broader shared distance layer`)

- [ ] **Step 7: Commit**

```bash
git add scripts/bench/run_simplewiki_openai_20k_100.sh results/high_dim_validation remote-logs/high_dim_validation
git commit -m "chore: refresh high-dim rbc batching evidence"
```
