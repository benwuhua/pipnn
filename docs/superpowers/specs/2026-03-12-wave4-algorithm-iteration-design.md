# Wave 4 Algorithm Iteration Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: shift the PiPNN PoC back from quality-harness work to paper-fidelity and algorithm-performance iteration

## 1. Goal

Wave 4 exists to answer one question: does a more paper-faithful PiPNN implementation improve the PoC enough to justify further investment?

This wave explicitly deprioritizes new quality-harness expansion. The active objective is algorithm iteration against the current PoC baseline.

## 2. Priority Order

1. `HashPrune` fidelity
2. `RBC` partition / overlap fidelity
3. `leaf_kNN` candidate-generation optimization
4. `1M/100` authority benchmark against the frozen current PoC

Why this order:
- first fix semantic drift versus the paper
- then improve candidate quality
- only then optimize the heavy path
- finally validate the combined result on the full authority benchmark

## 3. Benchmark Policy

Fast iteration:
- dataset: `SIFT1M`
- slice: `100k/200`

Authority validation:
- dataset: `SIFT1M`
- slice: `1M/100`

Stage gate:
- each stage may temporarily regress while under active development
- each completed stage must recover to `Recall@10 >= 0.95` on `100k/200`

Final gate:
- rerun the frozen current PoC on `1M/100` to establish the wave-4 baseline
- wave-4 final candidate must:
  - match or exceed that `1M/100` recall
  - improve `1M/100` build time versus that frozen baseline

## 4. Proposed Feature Split

### Feature 19 — HashPrune Fidelity

Objective:
- align bounded-memory prune semantics more closely to the paper

Expected work:
- clarify hash bucket semantics
- clarify replacement / tie-break semantics
- add paper-oriented micro tests for bucket occupancy and order-independence
- add instrumentation for kept / dropped candidate counts

Success:
- behavior is mechanically specified
- `100k/200` stage result returns to `Recall@10 >= 0.95`

### Feature 20 — RBC Fidelity

Objective:
- strengthen the current partitioning path so it is closer to paper intent

Expected work:
- revisit leader selection and overlap behavior
- expose leaf statistics and overlap statistics
- validate candidate-generation quality before touching later stages

Success:
- `100k/200` stage result returns to `Recall@10 >= 0.95`
- benchmark artifacts include leaf-count / overlap diagnostics

### Feature 21 — leaf_kNN Optimization

Objective:
- reduce the dominant candidate-generation cost without re-opening semantic ambiguity

Expected work:
- preserve the feature-19/20 semantics
- replace the current naive leaf scan with a more batch-friendly / block-oriented distance path
- measure `partition / leaf_knn / prune` stage times before and after

Success:
- `100k/200` stage result returns to `Recall@10 >= 0.95`
- `leaf_knn` stage time improves measurably

### Feature 22 — 1M Authority Benchmark

Objective:
- prove whether wave 4 beats the frozen current PoC where it matters

Expected work:
- rerun the current PoC once on `1M/100` and freeze that as baseline
- run the wave-4 candidate on the same authority slice
- compare build time, recall, qps, edge count, and stage timings

Success:
- recall is not below the frozen baseline
- build time is lower than the frozen baseline

## 5. Non-Goals

- no new mutation-scope expansion
- no new ST-harness expansion unless a repeated blocker forces it
- no full rewrite of the PoC graph builder
- no attempt in this wave to reproduce the paper’s final 1B-scale claim

## 6. Engineering Constraints

- keep the current PoC path runnable throughout
- avoid mixing semantic changes and optimization changes in one feature
- prefer additive instrumentation over ad hoc shell diagnosis
- all authority numbers continue to come from the remote x86 host

## 7. Deliverables

- updated SRS/design sections for wave 4
- features 19-22 appended to `feature-list.json`
- per-stage plans and benchmark artifacts
- updated benchmark report with a frozen `1M/100` current-PoC baseline and wave-4 comparison

## 8. Recommended Increment Matrix

| Change | Type | Affected Features | Action |
|---|---|---|---|
| Wave 4 algorithm iteration scope | New | none | add features 19-22 |
| HashPrune paper-fidelity requirements | New | none | add feature 19 |
| RBC paper-fidelity requirements | New | none | add feature 20 |
| leaf_kNN optimization requirements | New | none | add feature 21 |
| `1M/100` authority comparison requirement | New | none | add feature 22 |
| benchmark traceability text in SRS/design | Modified | feature 11 docs, benchmark report references | update docs in place |

## 9. Approval Boundary

Once approved, the next step is:
- create / consume `increment-request.json`
- update SRS and design in place for wave 4
- append features 19-22
- then return to `long-task-work`
