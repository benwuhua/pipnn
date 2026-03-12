# Feature 11 Plan: SIFT1M Benchmark Matrix

- Date: 2026-03-12
- Feature: #11 `100k/100、200k/100、500k/100 口径评测`
- Scope: Complete the fixed SIFT1M benchmark matrix and collect reproducible evidence for PiPNN vs standard hnswlib.

## Inputs

- SRS:
  - `NFR-001`: fixed benchmark scales `100k/100`, `200k/100`, `500k/100`
  - `NFR-002`: optimized PiPNN config must keep `recall_at_10 >= 0.95`
  - `NFR-003`: commands, logs, and artifacts must be traceable
- Design:
  - preferred route is `fanout=1 + replicas=2 + max_leaders=128 + beam=256`
  - regression scales are `100k/100`, `200k/100`, `500k/100`
  - remote execution uses `generic-x86-remote`

## Assumptions

- Remote x86 host remains reachable via `generic-x86-remote`
- Remote dataset path is `/data/work/knowhere-rs-src/data/sift`
- Existing 100k/100 and 200k/100 results remain valid unless rerun is needed to resolve inconsistency

## Steps

1. Re-run config gate with `SIFT1M_DIR=/data/work/knowhere-rs-src/data/sift` and confirm `generic-x86-remote check-env`.
2. Sync the local repository to a clean remote directory under `/data/work/pipnn`.
3. Run remote build and `ctest` to ensure previously passing features remain green.
4. Launch a reproducible `500k/100` benchmark job with:
   - PiPNN optimized config: `rbc_cmax=128`, `rbc_fanout=1`, `leader_frac=0.02`, `max_leaders=128`, `replicas=2`, `leaf_k=12`, `max_degree=32`, `hash_bits=12`, `beam=256`, `bidirected=1`
   - standard `hnswlib` baseline in the same script
5. Fetch the remote log and result JSON files back to `remote-logs/` and local `results/`.
6. Update the benchmark report with:
   - commands
   - log paths
   - `build_sec`, `recall_at_10`, `qps`, `edges`
   - profile breakdown when available
7. If `500k/100` meets `recall_at_10 >= 0.95`, mark feature 11 complete and prepare feature 12 evaluation. If it fails, keep feature 11 failing and capture the gap precisely.

## Verification

- `python3 scripts/check_configs.py feature-list.json --feature 11`
- `/Users/ryan/.codex/skills/generic-x86-remote/scripts/check-env.sh --json`
- Remote build: `cmake -S . -B build && cmake --build build -j`
- Remote tests: `ctest --test-dir build --output-on-failure`
- Remote benchmark: `./scripts/bench/remote_bench_sift1m.sh`

## Output Artifacts

- `remote-logs/`
- `results/pipnn_vs_hnsw_sift1m.md`
- `results/pipnn_metrics.json`
- `results/hnsw_metrics.json`
