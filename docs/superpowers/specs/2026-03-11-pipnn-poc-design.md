# PiPNN PoC Design (C++, SIFT1M, HNSW Baseline)

## 1. Scope and Goals

### Goals
- Validate `A`: `HashPrune` is practical for online pruning with bounded degree and order-independent output.
- Validate `B`: end-to-end graph build pipeline works: `RBC partition -> leaf exact kNN -> HashPrune -> optional final prune`.
- Validate `C`: on `SIFT1M`, graph build time is faster than standard `hnswlib` baseline while keeping acceptable ANN quality.

### Non-Goals (PoC phase)
- Full parity with paper-level optimization (e.g., deep SIMD tuning, GPU kernels, billion-scale runs).
- Production-hardening and API stability.
- Distributed build.

## 2. Approach Choice

Chosen approach: minimal viable algorithmic validation.

- Implement a single-machine C++ PoC with clear module boundaries.
- Use exact distance computations in leaves first (correctness first), then add micro-optimizations only if needed.
- Use `hnswlib` C++ as the standard baseline for construction speed and search quality comparison.

Rationale:
- Fastest path to de-risk paper claims in a controlled setup.
- Keeps implementation small enough for quick iteration while preserving core algorithm structure.

## 3. Architecture

### 3.1 Modules
- `data/`
  - SIFT1M loader (`fvecs`/`ivecs` parsing), train/base/query/ground-truth reader.
- `core/distance.h`
  - L2 distance primitives, optional SIMD switch points.
- `core/rbc.h`
  - Randomized Ball Carving with configurable fanout and leaf size.
- `core/leaf_knn.h`
  - Exact kNN construction per leaf (top-k by L2), emits candidate directed edges.
- `core/hashprune.h`
  - Per-node hashed reservoir, degree cap `Lmax`, deterministic tie-breaking.
- `core/pipnn_builder.h`
  - Orchestrates partitioning, candidate generation, prune-and-add, optional final prune.
- `core/graph.h`
  - CSR-like adjacency storage for ANN search.
- `search/greedy_beam.h`
  - Query-time graph search (greedy/beam), parameters for recall-latency tradeoff.
- `baseline/hnsw_runner.h`
  - Wraps `hnswlib` build/query benchmarking.
- `bench/runner.cpp`
  - End-to-end benchmark CLI and result report.

### 3.2 Build System
- `CMake` project layout.
- Third-party:
  - `hnswlib` as submodule or vendored header.

## 4. Data Flow

1. Load SIFT1M vectors and ground-truth.
2. Build PiPNN graph:
   - Partition dataset with RBC into overlapping leaves.
   - Build exact leaf kNN edges.
   - Stream candidate edges to per-node HashPrune reservoirs.
   - Materialize final adjacency; optionally run final prune.
3. Build HNSW baseline index with matched metric and comparable graph degree/search settings.
4. Query both indexes on same query set.
5. Collect metrics:
   - Build time (wall-clock).
   - Peak RSS (optional if available).
   - Recall@10.
   - QPS at fixed search params.
6. Emit markdown/json summary.

## 5. Key Algorithms and Contracts

### 5.1 HashPrune Contract
- Input: stream of candidate neighbors for node `p`.
- State: hashmap `bucket -> best candidate in bucket`, plus max-heap or tracked farthest for eviction.
- Output invariant:
  - Degree `<= Lmax`.
  - Deterministic output independent of insertion order (given fixed random seed/hyperplanes and tie-break rule).

### 5.2 RBC Contract
- Produces overlapping leaves with size bounded by `Cmax`.
- Fanout controls overlap: each point assigned to nearest `f(depth)` leaders.

### 5.3 Leaf kNN Contract
- For each leaf, compute top-`k_leaf` nearest neighbors per point among leaf members.
- Emit bi-directed edges into candidate stream.

## 6. Configuration (Initial Defaults)

- `dim = 128` (SIFT1M)
- `Cmax = 4096`
- `leader_frac = 0.02`
- `fanout(depth) = 2` (constant PoC default)
- `k_leaf = 24`
- `hash_bits = 12`
- `Lmax = 64`
- `final_prune = off` (phase 1), optional in phase 2
- Search:
  - PiPNN beam `L_search = 64`
  - HNSW `efSearch` tuned to similar recall band for fair QPS compare

## 7. Evaluation Plan and Acceptance Criteria

### Required checks
- Correctness:
  - Unit tests for parser, distance, HashPrune invariants, RBC leaf bound.
  - Determinism test: shuffled candidate insertion yields identical HashPrune result.
- Quality:
  - `Recall@10 >= 0.90` target on SIFT1M query set (with tuned search params).
- Performance:
  - Build-time speedup over HNSW: target `>= 1.5x` (same machine/settings discipline).

### Reporting
- One benchmark table with:
  - Build seconds, Recall@10, QPS, index edge count, memory estimate.
- Include command lines and seeds for reproducibility.

## 8. Error Handling and Risk Controls

- Parser failures: hard fail with file/offset diagnostics.
- Numeric safety: reject dimension mismatch and NaN distances in debug builds.
- Degenerate partitions: fallback to splitting with lower fanout or random reassignment.
- Large-memory spikes:
  - Process leaves in chunks.
  - Avoid storing full candidate universe globally.

Risks:
- Recall may lag at initial defaults.
  - Mitigation: tune `k_leaf`, `fanout`, `Lmax`, and search beam.
- HNSW comparison fairness.
  - Mitigation: report full parameter sets and perform recall-matched QPS.

## 9. Milestones

1. Skeleton + CMake + SIFT loader + benchmark harness.
2. HashPrune + tests (including order-independence test).
3. RBC + leaf kNN + graph materialization.
4. PiPNN query path and first end-to-end run on small subset.
5. Full SIFT1M benchmark vs `hnswlib`.
6. Parameter sweep for meeting acceptance criteria.

## 10. Deliverables

- C++ PoC source tree.
- Reproducible benchmark command(s).
- `results/pipnn_vs_hnsw_sift1m.md` summary.

## 11. Remote x86 Execution Plan

Compile and benchmark runs should execute on remote x86_64 Linux host.

- Skill/workflow: `generic-x86-remote` scripts under `/Users/ryan/.codex/skills/generic-x86-remote/scripts/`.
- Required config: `$GENERIC_X86_REMOTE_ENV` or `~/.config/generic-x86-remote/remote.env`.
- Standard execution sequence:
  1. `check-env.sh` to verify `arch=x86_64`, CPU features, compiler/CMake availability.
  2. `sync.sh` to mirror PoC repository to remote workdir.
  3. `run.sh` for short build/test commands.
  4. `run-bg.sh` for long SIFT1M benchmark jobs with persisted logs.
  5. `fetch.sh` to collect logs and benchmark artifacts back to local repo.
- Reproducibility rule:
  - All reported benchmark numbers must include the remote machine info and captured command lines.
