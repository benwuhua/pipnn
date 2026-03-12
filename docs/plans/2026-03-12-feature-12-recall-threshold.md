# Feature 12 Plan: Reach NFR-002 Recall Threshold

- Date: 2026-03-12
- Feature: #12 `NFR-002 召回阈值达标`
- Target: use one optimized PiPNN configuration that achieves `recall_at_10 >= 0.95` on `100k/100`, `200k/100`, and `500k/100` under subset-internal truth.

## Inputs

- SRS `NFR-002`: optimized config must satisfy `recall_at_10 >= 0.95` at `100k/100`, `200k/100`, `500k/100`
- Design recommendation: start from `fanout=1 + replicas=2 + max_leaders=128 + beam=256`
- Current evidence:
  - `100k/100`: recall `0.979`
  - `200k/100`: recall `0.963`
  - `500k/100`: recall `0.943`

## Root-Cause Hypotheses

1. Search budget is the primary limiter at `500k/100`
   - Evidence: at `100k/100`, increasing `beam` from `128` to `256` moved recall from `0.940` to `0.979`
   - Smallest fix candidate: increase `beam` while keeping graph-build params unchanged
2. Candidate coverage becomes insufficient at `500k/100`
   - If higher `beam` is not enough, increase candidate diversity with `replicas` or `max_leaders`

## Experiment Order

1. `500k/100`, `beam=384`, keep:
   - `cmax=128`
   - `fanout=1`
   - `leader_frac=0.02`
   - `max_leaders=128`
   - `replicas=2`
   - `leaf_k=12`
   - `max_degree=32`
   - `hash_bits=12`
   - `bidirected=1`
2. If recall still `< 0.95`, run `500k/100`, `beam=512`
3. If recall still `< 0.95`, run `500k/100`, `replicas=3`, `beam=256`
4. If recall still `< 0.95`, run `500k/100`, `replicas=3`, `beam=384`

## Stop Condition

- As soon as one config reaches `recall_at_10 >= 0.95` at `500k/100`, rerun that exact config at:
  - `100k/100`
  - `200k/100`
- If both smaller scales also meet the threshold, promote that config as the new optimized default for feature 12.

## Outputs

- Result JSON files under `results/feature12_*`
- Remote logs under `remote-logs/`
- Report update in `results/pipnn_vs_hnsw_sift1m.md`
- Feature 12 status update in `feature-list.json` and `task-progress.md`
