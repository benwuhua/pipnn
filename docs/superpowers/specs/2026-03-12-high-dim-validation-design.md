# High-Dimension Validation Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: add one high-dimensional dataset path to validate whether the current PiPNN leaf-building architecture behaves more like the paper on high-dimensional data than it does on `SIFT1M`

## 1. Goal

The immediate goal is not to close feature 21. The immediate goal is to answer one narrower question:

> Is the current leaf-building architecture underperforming because `SIFT1M` is the wrong workload, or because the current per-leaf optimization direction is fundamentally wrong?

To answer that, this design adds one high-dimensional validation dataset to the PoC, using a path that already exists on the remote x86 host.

## 2. Recommended Validation Dataset

### Option A — Recommended: `OpenAI-ArXiv`

Use a high-dimensional `L2` dataset available on the remote host.

Why this is recommended:
- high-dimensional vectors are closer to the paper's motivating workloads than `SIFT1M`
- `L2` fits the current codebase without introducing a new metric implementation
- it isolates the leaf-building question without widening scope into MIPS support
- it is faster to operationalize than adding `DEEP` support first

### Option B — `Wikipedia-Cohere`

Trade-off:
- larger and high-dimensional, but requires `MIPS`, which would mix metric work into this validation branch

### Option C — `DEEP`

Trade-off:
- closer to the paper's billion-scale story, but requires new dataset-format plumbing first and is slower to bring online

## 3. Scope

This branch will:
- add one new dataset route for a high-dimensional `L2` dataset
- add the minimum binary readers required for the chosen remote dataset files
- run PiPNN and HNSW on a small repeatable slice of that dataset
- compare stage timings against the existing `SIFT1M` results

This branch will not:
- add `MIPS`
- add `DEEP` / BigANN dataset format support
- attempt to close the `1M/100` authority benchmark
- modify `RBC` or `HashPrune`

## 4. Current Environment Facts

The current code only supports:
- `synthetic`
- `sift1m`

The current remote host already contains candidate high-dimensional datasets under `/data/work/datasets/`, including:
- `/data/work/datasets/openai-arxiv`
- `/data/work/datasets/wikipedia-cohere-1m`
- `/data/work/datasets/simplewiki-openai/...`

Therefore, the fastest path is to add a dataset reader for the file format used by the `OpenAI-ArXiv` remote copy and validate on that.

## 5. Architecture

### 5.1 Dataset Layer

Add a small, focused binary reader module for the format used by the chosen high-dimensional dataset.

Requirements:
- keep the current `sift_reader` intact
- avoid over-generalizing to all BigANN-family formats in this round
- support the minimum files needed for one validation dataset:
  - base vectors
  - query vectors
  - optional truth file

The design should prefer a new reader file over inflating `sift_reader.cpp` with multiple incompatible binary conventions.

### 5.2 CLI Layer

Add one new dataset keyword, for example:
- `openai-arxiv`

Behavior:
- route to the new binary reader
- preserve the current `synthetic|sift1m` behavior unchanged
- continue to require explicit `--base` and `--query` inputs

### 5.3 Benchmark Layer

Add a dedicated benchmark wrapper for the high-dimensional dataset.

The wrapper should:
- run on the remote x86 host
- use a fixed slice size for quick iteration
- produce the same artifact shape used elsewhere:
  - `pipnn_metrics.json`
  - `hnsw_metrics.json` if applicable
  - captured stdout/profile output

## 6. Validation Plan

### Phase A — Data plumbing smoke

Success criteria:
- the new dataset can be loaded successfully
- PiPNN can build and search on a small slice
- HNSW can run on the same slice

### Phase B — High-dimensional leaf validation

Run the same wave-4 style configuration on the new dataset.

Evaluate:
- `build_sec`
- `recall_at_10`
- `qps`
- `partition_sec`
- `leaf_knn_sec`
- `prune_sec`

The core question is whether high dimension improves the relative viability of the leaf-building strategy.

## 7. Decision Rule

This branch is successful if it lets us make one of these two decisions with evidence:

1. `feature 21` is still promising on high-dimensional data
- then continue algorithm work on the leaf path using the high-dimensional dataset as the primary validation set

2. `feature 21` still regresses badly on high-dimensional data
- then stop pushing per-leaf kernel work and redesign around a different batching or builder-level strategy

## 8. Risks

### Risk 1 — File format mismatch

Mitigation:
- inspect the remote dataset files first
- write a tiny format probe before fully wiring the CLI route

### Risk 2 — Dataset copy is incomplete or undocumented

Mitigation:
- make the wrapper take explicit `--base --query --truth` values even for the named dataset route
- avoid baking remote paths into the core code

### Risk 3 — We accidentally widen scope into metric work

Mitigation:
- keep this branch on `L2` only
- defer `Wikipedia-Cohere` and `MIPS`

## 9. Deliverables

- one new design/reader path for a high-dimensional `L2` dataset
- CLI support for that dataset
- one remote benchmark script
- one benchmark result bundle comparing behavior against the current `SIFT1M` understanding
- an explicit decision note in `task-progress.md`

## 10. Approval Boundary

Once approved, the next step is to write an implementation plan and then execute the new dataset path through TDD before returning to the feature-21 algorithm question.
