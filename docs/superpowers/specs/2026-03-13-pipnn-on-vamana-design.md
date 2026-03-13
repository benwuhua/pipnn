# PiPNN-on-Vamana Design

- Date: 2026-03-13
- Status: Draft for user review
- Scope: replace the current PiPNN full-stack PoC mainline with a converged architecture that uses PiPNN only for candidate generation and relies on a native Vamana/DiskANN path for final graph refinement and search

## 1. Problem

The current repository compares:

- a custom PiPNN graph build plus custom beam search
- a standard `hnswlib` baseline

This comparison is useful for trend discovery, but it does not isolate where PiPNN adds value. Build logic, graph semantics, and query path all differ at once.

The new design narrows the question:

> If PiPNN is used only to propose candidate neighbors or an initial graph, does it still add value when final refinement and search are delegated to a native Vamana/DiskANN implementation?

## 2. Recommendation

Use a `PiPNN-on-Vamana` architecture:

- PiPNN generates candidate neighbors or an initial graph
- Vamana/DiskANN refines that candidate set into the final graph
- native Vamana search is the only query path used for the new mainline benchmark

Do not use `hnswlib` as the integration target for this phase.

Reason:

- HNSW search semantics are tightly coupled to the HNSW graph hierarchy and entry routing
- Vamana is a flat graph plus greedy or beam-like search, which is much closer to the current PiPNN output shape
- this reduces interface mismatch and makes benchmark conclusions easier to interpret

## 3. Open-Source Base

Recommended upstream base:

- `microsoft/DiskANN`
- preferred integration target for the current C++ project: `cpp_main`

Why `cpp_main`:

- the current repository is constrained to `C++ + CMake`
- switching to Rust at the same time would confound architecture, implementation language, and benchmark conclusions

Why not Rust for the current mainline:

- Rust is technically possible
- Rust is not recommended for the current mainline because it changes too many variables at once
- a Rust spike may be useful later, but it should be treated as a parallel feasibility track, not as the primary implementation path

## 4. Design Goals

Primary goals:

- isolate the value of PiPNN candidate generation
- converge onto a mature search path
- remove the current "custom build plus custom search" coupling from the main conclusion path
- evaluate only on full original datasets for final claims

Non-goals:

- do not rewrite the entire project in Rust
- do not preserve the current custom search path as a co-equal mainline benchmark mode
- do not use small slices as final evidence for whether PiPNN is worthwhile

## 5. Benchmark Policy

### 5.1 Dataset roles

Use three dataset tiers:

- `SIFT1M full`
  - role: standard ANN reference benchmark
  - use: full original base, full original query set, official ground truth
- `wikipedia-cohere-1m full`
  - role: primary decision dataset
  - use: full original base, full original query set, official ground truth
- `simplewiki-openai`
  - role: high-dimensional development and smoke dataset only
  - use: debugging, instrumentation, and fast validation

### 5.2 Slice policy

Examples such as `20k/100` and `100k/200` are allowed only for:

- functional debugging
- profiling
- local trend checks

They must not be used as the final basis for deciding whether PiPNN has an advantage.

### 5.3 Comparison modes

The benchmark harness should eventually compare:

- `vamana_native`
- `pipnn_vamana`
- `hnsw_baseline`

Interpretation policy:

- the primary decision is `vamana_native` vs `pipnn_vamana`
- `hnsw_baseline` remains a reference point, not the core architecture decision metric

## 6. Architecture

### 6.1 Logical flow

```text
vectors
  -> PiPNN candidate generation
  -> Vamana refine / prune / link
  -> final graph
  -> native Vamana search
  -> metrics
```

This replaces the current mainline shape:

```text
vectors
  -> PiPNN final graph
  -> custom search
  -> metrics
```

### 6.2 Module split

Recommended new boundaries:

- `CandidateGenerator`
  - responsibility: generate candidate neighbors for each point
  - source of logic: current `RBC + shortlist + exact score` path
- `GraphRefiner`
  - responsibility: produce the final bounded graph from candidate neighbors
  - source of logic: native Vamana/DiskANN refinement semantics
- `SearchAdapter`
  - responsibility: run the canonical query path
  - source of logic: native Vamana search semantics
- `BenchmarkHarness`
  - responsibility: run comparable modes and report the same metrics

### 6.3 Boundary rule

The only intended semantic difference between `vamana_native` and `pipnn_vamana` is:

- how the candidate neighbors or initial graph are proposed

The following should be shared whenever feasible:

- final graph refinement policy
- search policy
- evaluation code

This is the main mechanism that keeps the benchmark interpretable.

## 7. Repository Mapping

Current files that should remain legacy or be demoted from the mainline:

- `src/core/pipnn_builder.cpp`
- `src/search/greedy_beam.cpp`

Recommended new files:

- `src/candidates/pipnn_candidate_generator.h`
- `src/candidates/pipnn_candidate_generator.cpp`
- `src/refine/vamana_refiner.h`
- `src/refine/vamana_refiner.cpp`
- `src/search/vamana_search_adapter.h`
- `src/search/vamana_search_adapter.cpp`

Recommended benchmark mode additions:

- `--mode vamana`
- `--mode pipnn_vamana`

Legacy modes may remain temporarily for transition:

- `--mode pipnn`
- `--mode hnsw`

## 8. Phase Plan

### Phase 1: Candidate isolation

Goal:

- extract the current candidate-generation portion from the existing PiPNN builder

Deliverable:

- a reusable candidate generator that does not directly emit the final graph

### Phase 2: Vamana native path

Goal:

- establish a native Vamana baseline path in the repository

Deliverable:

- a working `vamana_native` mode with shared metric reporting

### Phase 3: PiPNN-on-Vamana path

Goal:

- feed PiPNN-generated candidates into the same Vamana refine path

Deliverable:

- a working `pipnn_vamana` mode

### Phase 4: Full-dataset benchmark

Goal:

- determine whether PiPNN candidate generation provides real value on full datasets

Deliverable:

- full-dataset comparison on:
  - `SIFT1M`
  - `wikipedia-cohere-1m`

## 9. Success Criteria

This design is successful only if the final benchmark can answer:

- does PiPNN improve build time, recall, memory, or graph size under a fixed Vamana search path?

The project should continue on this mainline only if:

- `pipnn_vamana` shows stable value on at least one full dataset metric that matters

If not:

- stop investment in PiPNN as a full-stack direction
- keep the result as a negative but useful research conclusion

## 10. Risks

### Risk 1: Candidate generator does not help after Vamana refinement

Meaning:

- PiPNN may help only when paired with its own graph semantics, not when used as a front-end proposal stage

Mitigation:

- treat this as a legitimate outcome and terminate the mainline if the effect disappears

### Risk 2: Vamana refinement semantics are not cleanly separable

Meaning:

- some open-source code paths may combine candidate generation and refinement too tightly

Mitigation:

- keep Phase 1 focused on a minimal, inspectable in-memory path
- avoid broad codebase adoption before the data path is understood

### Risk 3: Rust becomes a distraction

Meaning:

- moving to Rust before architecture convergence would mix tooling and algorithm questions

Mitigation:

- keep Rust out of the primary path
- allow only an isolated spike later if the C++ convergence path proves worthwhile

## 11. Approval Boundary

If this design is approved, the next step is:

- write an implementation plan for Phase 1 through Phase 4
- keep Rust as a later parallel spike, not as the immediate implementation base
