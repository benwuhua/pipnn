# Feature 12 Test Cases — Recall Threshold

- Feature ID: 12
- Feature Title: `NFR-002 召回阈值达标`
- Date: 2026-03-12
- Tester: Codex

## Scope

Validate that one optimized PiPNN configuration satisfies `recall_at_10 >= 0.95` on the fixed subset-truth SIFT1M scales `100k/100`, `200k/100`, and `500k/100`.

## Fixed Configuration Under Test

- `cmax=128`
- `fanout=1`
- `leader_frac=0.02`
- `max_leaders=128`
- `replicas=2`
- `leaf_k=12`
- `max_degree=32`
- `hash_bits=12`
- `beam=384`
- `bidirected=1`

## Test Cases

### TC-12-001 — 100k/100 recall meets threshold

- Given the fixed configuration above
- When running `100k/100` with subset-internal truth
- Then `recall_at_10 >= 0.95`

Evidence:
- `results/feature12_100k_beam384.json`
- `remote-logs/feature12-100k-beam384_20260312T014340Z.log`

Observed:
- `recall_at_10 = 0.993`

### TC-12-002 — 200k/100 recall meets threshold

- Given the fixed configuration above
- When running `200k/100` with subset-internal truth
- Then `recall_at_10 >= 0.95`

Evidence:
- `results/feature12_200k_beam384.json`
- `remote-logs/feature12-200k-beam384_20260312T014607Z.log`

Observed:
- `recall_at_10 = 0.978`

### TC-12-003 — 500k/100 recall meets threshold

- Given the fixed configuration above
- When running `500k/100` with subset-internal truth
- Then `recall_at_10 >= 0.95`

Evidence:
- `results/feature12_500k_beam384.json`
- `remote-logs/feature12-500k-beam384_20260312T013522Z.log`

Observed:
- `recall_at_10 = 0.962`

## Summary

- Executed: 3
- Passed: 3
- Failed: 0
