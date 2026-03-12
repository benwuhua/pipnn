# PiPNN vs HNSW Benchmark (Status)

## Current status

- PoC pipeline implemented in C++: `RBC + leaf-kNN + HashPrune + best-first beam search`.
- Baseline is standard `hnswlib` C++ (`HierarchicalNSW`).
- Local tests all pass.
- Remote x86 benchmark is repeatable via `generic-x86-remote`.

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

### 500k base / 100 query (subset-internal truth)

This run uses direct binary invocation without `--truth`, so recall is computed against exact top-k on the loaded 500k subset.

| config | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| fanout=1, replicas=2, max_leaders=128, beam=256 | 401.906 | 0.943 | 219.942 | 13219687 |
| hnsw baseline | 1031.69 | 0.988 | 983.734 | 16000000 |

Profile for the 500k optimized run:
- `partition_sec=110.784`
- `leaf_knn_sec=245.780` (dominant)
- `prune_sec=45.213`
- `leaves=21325`
- `candidate_edges=23826016`

Notes:
- The current optimized PiPNN config misses the `recall_at_10 >= 0.95` threshold at 500k by `0.007`.
- Remote host had an unrelated long-running `knowhere` benchmark consuming CPU during this run, so absolute build times should be treated as noisy.
- 500k log files:
  - `remote-logs/pipnn-500k100-subset_20260312T005341Z.log`
  - `remote-logs/hnsw-500k100-subset_20260312T010152Z.log`

## Final quality config for NFR-002

To satisfy the recall threshold across all three scales, keep the graph-build parameters unchanged and increase search beam from `256` to `384`.

| scale | build_sec | recall_at_10 | qps | edges |
|---|---:|---:|---:|---:|
| 100k/100 | 74.6352 | 0.993 | 214.342 | 2657240 |
| 200k/100 | 155.867 | 0.978 | 178.467 | 5345290 |
| 500k/100 | 401.321 | 0.962 | 152.697 | 13219687 |

Fixed config:
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

Feature 12 evidence logs:
- `remote-logs/feature12-100k-beam384_20260312T014340Z.log`
- `remote-logs/feature12-200k-beam384_20260312T014607Z.log`
- `remote-logs/feature12-500k-beam384_20260312T013522Z.log`

## Reproducibility Bundle

Single audit entry point:
- `results/repro_manifest.json`

Manifest validation:

```bash
python3 scripts/validate_repro_manifest.py results/repro_manifest.json
```

The manifest records:
- remote environment and sync/build/test/fetch commands
- remote dataset and repo paths
- one run entry per tracked benchmark
- local and remote log paths
- local and remote result paths
- copied key metrics for direct audit

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
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake -S . -B build
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake --build build -j
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- ctest --test-dir build --output-on-failure

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

## 500k subset-truth direct commands

```bash
# PiPNN optimized config, omit --truth to compute exact top-k on the loaded subset
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run-bg.sh --repo /data/work/pipnn --name pipnn-500k100-subset -- env PIPNN_PROFILE=1 PIPNN_ECHO_CONFIG=1 ./build/pipnn --mode pipnn --dataset sift1m --metric l2 --base /data/work/knowhere-rs-src/data/sift/base.fvecs --query /data/work/knowhere-rs-src/data/sift/query.fvecs --max-base 500000 --max-query 100 --rbc-cmax 128 --rbc-fanout 1 --leader-frac 0.02 --max-leaders 128 --replicas 2 --leaf-k 12 --leaf-scan-cap 0 --max-degree 32 --hash-bits 12 --beam 256 --bidirected 1 --output results/bench_500k_100_subset/pipnn_metrics.json

# HNSW baseline, also omit --truth for the same subset-internal recall definition
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run-bg.sh --repo /data/work/pipnn --name hnsw-500k100-subset -- ./build/pipnn --mode hnsw --dataset sift1m --metric l2 --base /data/work/knowhere-rs-src/data/sift/base.fvecs --query /data/work/knowhere-rs-src/data/sift/query.fvecs --max-base 500000 --max-query 100 --output results/bench_500k_100_subset/hnsw_metrics.json
```
