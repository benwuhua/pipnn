# PiPNN-on-Vamana Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a converged `PiPNN-on-Vamana` path where PiPNN provides candidate neighbors, Vamana/DiskANN semantics produce the final graph, and native Vamana search becomes the mainline query path.

**Architecture:** The implementation is split into three layers: a PiPNN candidate generator extracted from the current builder, a Vamana graph/refine layer, and a shared benchmark harness that compares `vamana_native` and `pipnn_vamana` under the same search semantics. Full-dataset benchmarking stays separate from the initial code integration so architecture convergence can happen before long-running experiments.

**Tech Stack:** C++20, CMake, ctest, existing `pipnn_core` library, standard `hnswlib` baseline, future DiskANN `cpp_main` integration.

---

## File Structure

### Files to create

- `src/candidates/pipnn_candidate_generator.h`
- `src/candidates/pipnn_candidate_generator.cpp`
- `src/refine/vamana_refiner.h`
- `src/refine/vamana_refiner.cpp`
- `src/search/vamana_search_adapter.h`
- `src/search/vamana_search_adapter.cpp`
- `tests/test_pipnn_candidate_generator.cpp`
- `tests/test_vamana_refiner.cpp`
- `tests/test_vamana_search_adapter.cpp`

### Files to modify

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/bench/runner.h`
- `src/bench/runner.cpp`
- `src/cli/app.cpp`
- `src/core/pipnn_builder.h`
- `src/core/pipnn_builder.cpp`
- `tests/test_cli.cpp`
- `tests/test_cli_app.cpp`
- `tests/test_pipnn_integration.cpp`
- `tests/test_runner_metrics.cpp`

### Responsibility map

- `pipnn_candidate_generator.*`
  - extracts and owns `RBC + shortlist + exact scoring`
  - does not perform final graph pruning
- `vamana_refiner.*`
  - owns candidate-to-final-graph refinement with bounded degree
  - starts with a repository-local in-memory implementation shaped for later DiskANN alignment
- `vamana_search_adapter.*`
  - owns the canonical search path for both `vamana_native` and `pipnn_vamana`
- `pipnn_builder.*`
  - remains as a legacy convenience wrapper during transition
- `runner.*`
  - becomes the single mode router for `pipnn`, `vamana`, `pipnn_vamana`, and `hnsw`

## Chunk 1: Candidate Isolation

### Task 1: Add a candidate-generator test harness

**Files:**
- Create: `tests/test_pipnn_candidate_generator.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_pipnn_candidate_generator.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "candidates/pipnn_candidate_generator.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {0.0f, 1.0f},
      {1.0f, 1.0f},
  };

  pipnn::PipnnCandidateParams params;
  params.rbc.cmax = 2;
  params.rbc.fanout = 1;
  params.leaf_k = 2;

  pipnn::PipnnCandidateStats stats;
  auto candidates = pipnn::BuildPipnnCandidates(points, params, &stats);

  assert(candidates.size() == points.size());
  assert(stats.num_leaves > 0);
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build --output-on-failure -R '^pipnn_candidate_generator$'`
Expected: FAIL because the test target or candidate generator symbols do not exist yet.

- [ ] **Step 3: Register the test target**

Add `test_pipnn_candidate_generator` to `tests/CMakeLists.txt` linked against `pipnn_core`.

- [ ] **Step 4: Re-run test to verify it still fails for the right reason**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^pipnn_candidate_generator$'`
Expected: FAIL with missing header or undefined symbol, not with a harness error.

- [ ] **Step 5: Commit**

```bash
git add tests/CMakeLists.txt tests/test_pipnn_candidate_generator.cpp
git commit -m "test: add pipnn candidate generator harness"
```

### Task 2: Extract candidate-generation types and implementation

**Files:**
- Create: `src/candidates/pipnn_candidate_generator.h`
- Create: `src/candidates/pipnn_candidate_generator.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/core/pipnn_builder.cpp`
- Modify: `src/core/pipnn_builder.h`
- Test: `tests/test_pipnn_candidate_generator.cpp`

- [ ] **Step 1: Define the new candidate interfaces**

Add:

```cpp
struct PipnnCandidateStats {
  double partition_sec = 0.0;
  double score_sec = 0.0;
  std::size_t num_leaves = 0;
  std::size_t candidate_edges = 0;
  std::size_t rbc_assignment_total = 0;
  std::size_t rbc_points_with_overlap = 0;
};

struct PipnnCandidateParams {
  RbcParams rbc;
  int replicas = 1;
  int leaf_k = 24;
  bool bidirected = true;
  int candidate_cap = 0;
};

using CandidateAdjacency = std::vector<std::vector<int>>;

CandidateAdjacency BuildPipnnCandidates(const Matrix& points, const PipnnCandidateParams& params,
                                        PipnnCandidateStats* stats = nullptr);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^pipnn_candidate_generator$'`
Expected: FAIL because the implementation returns nothing useful yet.

- [ ] **Step 3: Write the minimal implementation**

Implementation rules:

- move `RBC + shortlist + ScoreShortlistExact` logic out of `BuildPipnnGraph`
- return per-node candidate adjacency before any final prune step
- preserve the existing profile statistics where possible
- keep `BuildPipnnGraph` working by calling the new candidate generator first and then the old prune path

- [ ] **Step 4: Run tests to verify the extraction passes**

Run: `ctest --test-dir build --output-on-failure -R '^(pipnn_candidate_generator|pipnn_integration|runner_metrics)$'`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/candidates/pipnn_candidate_generator.h src/candidates/pipnn_candidate_generator.cpp src/core/pipnn_builder.h src/core/pipnn_builder.cpp tests/test_pipnn_candidate_generator.cpp
git commit -m "feat: extract pipnn candidate generation"
```

## Chunk 2: Vamana Local Path

### Task 3: Add a refiner test harness

**Files:**
- Create: `tests/test_vamana_refiner.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_vamana_refiner.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "refine/vamana_refiner.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {{0.0f}, {1.0f}, {2.0f}, {3.0f}};
  pipnn::CandidateAdjacency candidates = {
      {1, 2, 3},
      {0, 2, 3},
      {1, 3, 0},
      {2, 1, 0},
  };

  pipnn::VamanaRefineParams params;
  params.max_degree = 2;

  auto graph = pipnn::RefineVamanaGraph(points, candidates, params);
  assert(graph.NumNodes() == 4);
  for (int i = 0; i < graph.NumNodes(); ++i) {
    assert(graph.Neighbors(i).size() <= 2);
  }
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build --output-on-failure -R '^vamana_refiner$'`
Expected: FAIL because the target and symbols do not exist.

- [ ] **Step 3: Register the test target**

Add `test_vamana_refiner` to `tests/CMakeLists.txt`.

- [ ] **Step 4: Re-run test to confirm the failure remains semantic**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^vamana_refiner$'`
Expected: FAIL due to missing implementation, not due to test registration.

- [ ] **Step 5: Commit**

```bash
git add tests/CMakeLists.txt tests/test_vamana_refiner.cpp
git commit -m "test: add vamana refiner harness"
```

### Task 4: Implement a repository-local Vamana-style refiner

**Files:**
- Create: `src/refine/vamana_refiner.h`
- Create: `src/refine/vamana_refiner.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_vamana_refiner.cpp`

- [ ] **Step 1: Define a minimal refiner interface**

Add:

```cpp
struct VamanaRefineParams {
  int max_degree = 64;
};

Graph RefineVamanaGraph(const Matrix& points, const CandidateAdjacency& candidates,
                        const VamanaRefineParams& params);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^vamana_refiner$'`
Expected: FAIL because the implementation is not present yet.

- [ ] **Step 3: Write the minimal implementation**

Implementation rules:

- start with a deterministic in-repo refinement path
- keep the algorithm simple and inspectable
- cap degree exactly
- use this as a transition seam toward DiskANN `cpp_main`, not as the final upstream-backed endpoint

- [ ] **Step 4: Run tests to verify it passes**

Run: `ctest --test-dir build --output-on-failure -R '^(vamana_refiner|pipnn_candidate_generator|pipnn_integration)$'`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/refine/vamana_refiner.h src/refine/vamana_refiner.cpp tests/test_vamana_refiner.cpp
git commit -m "feat: add local vamana refiner seam"
```

### Task 5: Add a native Vamana search adapter test

**Files:**
- Create: `tests/test_vamana_search_adapter.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_vamana_search_adapter.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "search/vamana_search_adapter.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {{0.0f}, {1.0f}, {2.0f}, {3.0f}};
  pipnn::Graph graph(4);
  graph.SetNeighbors(0, {1, 2});
  graph.SetNeighbors(1, {0, 2});
  graph.SetNeighbors(2, {1, 3});
  graph.SetNeighbors(3, {2});

  pipnn::VamanaSearchParams params;
  params.entry = 0;
  params.beam = 8;
  params.topk = 2;

  auto out = pipnn::SearchVamanaGraph(points, graph, pipnn::Vec{2.1f}, params);
  assert(!out.empty());
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build --output-on-failure -R '^vamana_search_adapter$'`
Expected: FAIL because the adapter does not exist.

- [ ] **Step 3: Register the test target**

Add `test_vamana_search_adapter` to `tests/CMakeLists.txt`.

- [ ] **Step 4: Re-run test to verify it still fails for the intended reason**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^vamana_search_adapter$'`
Expected: FAIL due to missing adapter implementation.

- [ ] **Step 5: Commit**

```bash
git add tests/CMakeLists.txt tests/test_vamana_search_adapter.cpp
git commit -m "test: add vamana search adapter harness"
```

### Task 6: Implement a canonical Vamana search adapter

**Files:**
- Create: `src/search/vamana_search_adapter.h`
- Create: `src/search/vamana_search_adapter.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_vamana_search_adapter.cpp`

- [ ] **Step 1: Define the adapter interface**

Add:

```cpp
struct VamanaSearchParams {
  int beam = 64;
  int topk = 10;
  int entry = 0;
};

std::vector<int> SearchVamanaGraph(const Matrix& points, const Graph& graph, const Vec& query,
                                   const VamanaSearchParams& params);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure -R '^vamana_search_adapter$'`
Expected: FAIL because the implementation is not present yet.

- [ ] **Step 3: Write the minimal implementation**

Implementation rules:

- initially reuse the current flat graph best-first behavior from `greedy_beam.cpp`
- move this behavior behind the Vamana-named seam
- do not delete the legacy path yet

- [ ] **Step 4: Run tests to verify it passes**

Run: `ctest --test-dir build --output-on-failure -R '^(vamana_search_adapter|pipnn_integration|runner_metrics)$'`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/search/vamana_search_adapter.h src/search/vamana_search_adapter.cpp tests/test_vamana_search_adapter.cpp
git commit -m "feat: add vamana search adapter seam"
```

## Chunk 3: Mode Plumbing

### Task 7: Add benchmark mode tests for `vamana` and `pipnn_vamana`

**Files:**
- Modify: `tests/test_cli_app.cpp`
- Modify: `tests/test_cli.cpp`
- Modify: `tests/test_runner_metrics.cpp`
- Modify: `tests/test_pipnn_integration.cpp`

- [ ] **Step 1: Write failing tests for new modes**

Add assertions that:

- `--mode vamana` writes metrics JSON
- `--mode pipnn_vamana` writes metrics JSON
- `RunBenchmark` returns sensible metrics for both modes

- [ ] **Step 2: Run the affected tests to verify failure**

Run: `ctest --test-dir build --output-on-failure -R '^(cli_app|cli|runner_metrics|pipnn_integration)$'`
Expected: FAIL because the new modes are not recognized.

- [ ] **Step 3: Commit the failing tests**

```bash
git add tests/test_cli_app.cpp tests/test_cli.cpp tests/test_runner_metrics.cpp tests/test_pipnn_integration.cpp
git commit -m "test: add vamana benchmark mode coverage"
```

### Task 8: Add runner and CLI support for `vamana` and `pipnn_vamana`

**Files:**
- Modify: `src/bench/runner.h`
- Modify: `src/bench/runner.cpp`
- Modify: `src/cli/app.cpp`
- Modify: `src/core/pipnn_builder.h`
- Modify: `src/core/pipnn_builder.cpp`

- [ ] **Step 1: Add configuration structs**

Add runner-level parameter structs for:

- Vamana refine
- Vamana search
- PiPNN candidate generation

- [ ] **Step 2: Run tests to verify they still fail**

Run: `ctest --test-dir build --output-on-failure -R '^(cli_app|cli|runner_metrics|pipnn_integration)$'`
Expected: FAIL because the runner path is not implemented yet.

- [ ] **Step 3: Implement `vamana` mode**

Mode behavior:

- build candidates using a simple native path
- refine with `RefineVamanaGraph`
- search with `SearchVamanaGraph`

- [ ] **Step 4: Implement `pipnn_vamana` mode**

Mode behavior:

- build candidates with `BuildPipnnCandidates`
- refine with `RefineVamanaGraph`
- search with `SearchVamanaGraph`

- [ ] **Step 5: Run targeted regression**

Run: `ctest --test-dir build --output-on-failure -R '^(pipnn_candidate_generator|vamana_refiner|vamana_search_adapter|cli_app|cli|runner_metrics|pipnn_integration)$'`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/bench/runner.h src/bench/runner.cpp src/cli/app.cpp src/core/pipnn_builder.h src/core/pipnn_builder.cpp tests/test_cli_app.cpp tests/test_cli.cpp tests/test_runner_metrics.cpp tests/test_pipnn_integration.cpp
git commit -m "feat: add vamana and pipnn_vamana modes"
```

## Chunk 4: Full Validation Preparation

### Task 9: Add benchmark scripts for the converged modes

**Files:**
- Create: `scripts/bench/run_vamana_sift1m_full.sh`
- Create: `scripts/bench/run_pipnn_vamana_sift1m_full.sh`
- Create: `scripts/bench/run_vamana_wiki_cohere_full.sh`
- Create: `scripts/bench/run_pipnn_vamana_wiki_cohere_full.sh`
- Modify: `results/high_dim_validation/README.md`

- [ ] **Step 1: Write script harness tests or smoke checks**

Use shell validation or repository script checks to ensure the commands reference:

- full dataset paths
- no `--max-base`
- no `--max-query` except when the original dataset definition requires it

- [ ] **Step 2: Run the script validation to verify failure if scripts are absent**

Run: `rg -n "run_vamana_.*_full|run_pipnn_vamana_.*_full" scripts/bench`
Expected: no matches before implementation.

- [ ] **Step 3: Write the scripts**

Implementation rules:

- full-dataset only
- use the unified benchmark modes
- keep output JSON naming parallel between `vamana` and `pipnn_vamana`

- [ ] **Step 4: Run a smoke validation**

Run: `bash -n scripts/bench/run_vamana_sift1m_full.sh`
Expected: exit code `0`

- [ ] **Step 5: Commit**

```bash
git add scripts/bench/run_vamana_sift1m_full.sh scripts/bench/run_pipnn_vamana_sift1m_full.sh scripts/bench/run_vamana_wiki_cohere_full.sh scripts/bench/run_pipnn_vamana_wiki_cohere_full.sh results/high_dim_validation/README.md
git commit -m "chore: add full-dataset vamana benchmark scripts"
```

### Task 10: Run focused verification before long benchmarks

**Files:**
- No source changes required

- [ ] **Step 1: Run local regression**

Run: `ctest --test-dir build --output-on-failure`
Expected: all tests pass, including the new candidate/refiner/search tests.

- [ ] **Step 2: Run remote x86 build and test**

Run:

```bash
/Users/ryan/.codex/skills/generic-x86-remote/scripts/sync.sh --src /Users/ryan/Code/Paper/pipnn --dest /data/work/pipnn --delete --exclude .git --exclude build --exclude build-cov --exclude build-mull --exclude .worktrees --exclude remote-logs
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake -S . -B build
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake --build build -j
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- ctest --test-dir build --output-on-failure
```

Expected: PASS on remote x86.

- [ ] **Step 3: Only after PASS, start full benchmarks**

Run the new full-dataset scripts in background and collect JSON outputs plus logs.

- [ ] **Step 4: Commit benchmark artifacts separately**

```bash
git add results/<relevant-paths>
git commit -m "chore: add pipnn-on-vamana benchmark artifacts"
```
