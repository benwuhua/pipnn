# Feature 11 Test Cases — Fixed SIFT1M Benchmark Matrix

- Feature ID: 11
- Feature Title: `100k/100、200k/100、500k/100 口径评测`
- Date: 2026-03-12
- Tester: Codex

## Scope

Validate that the benchmark report contains paired PiPNN/HNSW results for the fixed SIFT1M scales `100k/100`, `200k/100`, and `500k/100`, and that the supporting logs and JSON artifacts are traceable.

## Preconditions

- Remote benchmark host is configured through `generic-x86-remote`
- SIFT dataset path is `/data/work/knowhere-rs-src/data/sift`
- Local report path exists: `results/pipnn_vs_hnsw_sift1m.md`
- Local result paths exist:
  - `results/bench_500k_100_subset/pipnn_metrics.json`
  - `results/bench_500k_100_subset/hnsw_metrics.json`

## Test Cases

### TC-11-001 — 100k/100 pair exists in the report

- Given the benchmark report is updated
- When reading the `100k base / 100 query` section
- Then both PiPNN and HNSW rows are present with `build_sec`, `recall_at_10`, `qps`, and `edges`

Evidence:
- `results/pipnn_vs_hnsw_sift1m.md`

### TC-11-002 — 200k/100 pair exists in the report

- Given the benchmark report is updated
- When reading the `200k base / 100 query` section
- Then both PiPNN and HNSW rows are present with `build_sec`, `recall_at_10`, `qps`, and `edges`

Evidence:
- `results/pipnn_vs_hnsw_sift1m.md`

### TC-11-003 — 500k/100 pair and traceable artifacts exist

- Given the subset-truth `500k/100` runs completed
- When checking the report, result JSON files, and remote logs
- Then the report contains both PiPNN and HNSW rows, and the referenced JSON and log files exist

Evidence:
- `results/pipnn_vs_hnsw_sift1m.md`
- `results/bench_500k_100_subset/pipnn_metrics.json`
- `results/bench_500k_100_subset/hnsw_metrics.json`
- `remote-logs/pipnn-500k100-subset_20260312T005341Z.log`
- `remote-logs/hnsw-500k100-subset_20260312T010152Z.log`

## Summary

- Executed: 3
- Passed: 3
- Failed: 0
