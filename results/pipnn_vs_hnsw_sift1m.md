# PiPNN vs HNSW Benchmark (Status)

## Current status

- PoC pipeline implemented in C++: `RBC + leaf-kNN + HashPrune + best-first beam search`.
- Baseline is standard `hnswlib` C++ (`HierarchicalNSW`).
- Local tests all pass.
- Remote x86 benchmark is running and repeatable via `generic-x86-remote`.

## Local sanity run (synthetic, latest)

| mode | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| pipnn | 0.570125 | 0.995 | 1522.77 | 60367 |
| hnsw (hnswlib) | 0.685231 | 1.0 | 2155.74 | 64000 |

## Remote quick findings (SIFT subset)

Important: when using `--max-base`, evaluating against full SIFT1M groundtruth underestimates recall for both methods.  
For tuning, use subset-internal groundtruth (omit `--truth` so runner computes exact top-k on loaded subset).

### 30k base / 200 query (subset-internal truth)

| mode | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| pipnn (default params) | 40.5218 | 0.965 | 606.222 | 879404 |
| hnsw | 27.6905 | 0.999 | 64.215 | 960000 |

### 100k base / 200 query (subset-internal truth)

| mode | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| pipnn (default params) | 200.405 | 0.916 | 480.444 | 3119609 |
| hnsw | 120.752 | 0.997 | 19.6133 | 3200000 |

## Grid tuning (100k base / 100 query, subset-internal truth)

All runs on the same remote x86 host, serialized per run for fair build-time comparison.

| config | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| hnsw_baseline | 119.141 | 0.999 | 19.7555 | 3200000 |
| pipnn_fast_a | 27.1864 | 0.0000 | 24503.8 | 956244 |
| pipnn_fast_b | 37.9535 | 0.0000 | 17387.3 | 1490713 |
| pipnn_bal_a | 44.0091 | 0.0010 | 5484.22 | 1923869 |
| pipnn_quality_a | 200.074 | 0.9270 | 486.117 | 3119609 |
| pipnn_target_c | 198.033 | 0.9690 | 440.537 | 3188400 |
| pipnn_target_a | 224.980 | 0.9710 | 452.412 | 3186435 |
| pipnn_target_b | 250.720 | 0.9880 | 303.837 | 3977661 |
| pipnn_bal_b | 344.055 | 0.9780 | 288.096 | 4794754 |

Notes:
- `pipnn_quality_b` (`cmax=256, fanout=3, leaf_k=32, max_degree=64`) was aborted after >15 minutes; not cost-effective at this stage.
- Current implementation shows a strong trade-off:
  - Fast settings collapse recall.
  - High-recall settings are slower to build than HNSW at 100k scale.
- This points to implementation bottlenecks (partition and leaf exact-kNN cost), not just hyperparameter tuning.

## Optimized direction: fanout-1 with replicas

Implemented support for `replicas` (multiple independent partition passes with union of candidates) to recover quality while keeping fanout low.

### 30k base / 50 query

| config | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| fanout=2, replicas=1, max_leaders=1000, beam=128 | 47.3049 | 0.960 | 593.258 | 878674 |
| fanout=1, replicas=1, max_leaders=1000, beam=128 | 24.2140 | 0.014 | 2644.72 | 456678 |
| fanout=1, replicas=2, max_leaders=1000, beam=128 | 35.7222 | 0.918 | 615.652 | 772790 |
| fanout=1, replicas=3, max_leaders=1000, beam=128 | 53.2242 | 0.978 | 580.151 | 916800 |

### 100k base / 100 query (best tradeoff found)

| config | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| hnsw baseline | 119.141 | 0.999 | 19.7555 | 3200000 |
| fanout=1, replicas=2, max_leaders=128, beam=128 | 74.6286 | 0.940 | 513.467 | 2657240 |
| fanout=1, replicas=2, max_leaders=128, beam=256 | 74.4072 | 0.979 | 293.406 | 2657240 |

### 200k base / 100 query (trend validation)

| config | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| hnsw baseline | 320.146 | 0.995 | 1153.27 | 6400000 |
| fanout=1, replicas=2, max_leaders=128, beam=256 | 163.711 | 0.963 | 252.65 | 5345290 |

Profile for the 200k optimized run:
- `partition_sec=35.76`
- `leaf_knn_sec=106.18` (still dominant)
- `prune_sec=21.73`

### 30k base / 200 query (full SIFT1M truth, not recommended for subset tuning)

| mode | build_sec | recall_at_10 |
|---|---:|---:|
| pipnn (default) | 40.4858 | 0.0855 |
| hnsw | 26.9684 | 0.371 |

## Remote x86 commands

```bash
# 0) one-time setup
cp remote.env.example ~/.config/generic-x86-remote/remote.env
# edit ~/.config/generic-x86-remote/remote.env with real host values

# 1) check environment
/Users/ryan/.codex/skills/generic-x86-remote/scripts/check-env.sh --json

# 2) sync source
/Users/ryan/.codex/skills/generic-x86-remote/scripts/sync.sh --src /Users/ryan/Code/Paper/pipnn --dest /data/work/pipnn --delete --exclude .git --exclude build --exclude results

# 3) remote build + test
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure

# 4) SIFT1M benchmark background job
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run-bg.sh --repo /data/work/pipnn --name pipnn-sift1m -- ./scripts/bench/remote_bench_sift1m.sh

# 5) fetch logs
/Users/ryan/.codex/skills/generic-x86-remote/scripts/fetch.sh --src /data/work/logs --dest /Users/ryan/Code/Paper/pipnn/remote-logs
```

## Expected dataset files on remote

Under `$SIFT1M_DIR` (default `/data/datasets/sift1m`):
- `base.fvecs`
- `query.fvecs`
- `groundtruth.ivecs`


## Reuse knowhere remote config

```bash
./scripts/bench/reuse_knowhere_remote_env.sh
```
