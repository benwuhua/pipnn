# High-Dim Validation Notes

## Remote Probe

Probe artifacts:

- `results/openai_arxiv_probe/candidates.txt`
- `results/openai_arxiv_probe/files.txt`
- `results/openai_arxiv_probe/headers.txt`

Findings:

- `/data/work/datasets/openai-arxiv` exists but has no directly usable `fbin/ibin/fvecs/ivecs` files.
- `/data/work/datasets/simplewiki-openai/pipnn_public_small` contains:
  - `base.fbin`: `20000 x 3072`
  - `query.fbin`: `100 x 3072`
  - `gt.ibin`: `100 x 10`
- `/data/work/datasets/crisp/simplewiki-openai` contains:
  - `base.fvecs`
  - `query.fvecs`
  - `groundtruth.ivecs`
- `/data/work/datasets/wikipedia-cohere-1m` contains:
  - `base.fbin`: `1000000 x 768`
  - `query.fbin`: `5000 x 768`
  - `gt.ibin`: `5000 x 100`

Conclusion:

- The original `openai-arxiv` reader plan is not the shortest path.
- We can already validate a high-dimensional workload with the existing `fvecs/ivecs` reader by using `crisp/simplewiki-openai`.
- The CLI now also supports a generic file-backed dataset mode:
  - `--dataset file`
  - float vectors: `.fvecs`, `.fbin`
  - truth/int vectors: `.ivecs`, `.ibin`
- This unlocks direct use of `wikipedia-cohere-1m` without adding a dataset-specific parser.

## Full-Dataset Entry

Primary high-dimensional full-dataset entry:

- `scripts/bench/run_wikipedia_cohere_1m_full.sh`

Default dataset binding:

- base: `/data/work/datasets/wikipedia-cohere-1m/base.fbin`
- query: `/data/work/datasets/wikipedia-cohere-1m/query.fbin`
- truth: `/data/work/datasets/wikipedia-cohere-1m/gt.ibin`

Default compared modes:

- `pipnn_vamana`
- `vamana`

Default scope:

- full dataset when `MAX_BASE=0` and `MAX_QUERY=0`

## Authority 1M/100 Entry

For the current authority checkpoint, the reproducible `PiPNN-on-Vamana` entry is:

- `scripts/bench/run_wikipedia_cohere_1m_100_pipnn_vamana.sh`

Default scope:

- full base (`MAX_BASE=0`)
- first `100` official queries (`MAX_QUERY=100`)
- mode: `pipnn_vamana`

Rationale:

- the current repository-local `vamana` seam still uses an exact native candidate path
- that baseline path is not suitable for `1M` scale, so the authority run is currently tracked on the `pipnn_vamana` side first

## High-Dim Smoke

Dataset:

- base: `/data/work/datasets/crisp/simplewiki-openai/base.fvecs`
- query: `/data/work/datasets/crisp/simplewiki-openai/query.fvecs`
- metric: `l2`
- validation mode: subset-internal exact truth

Wrapper:

- `scripts/bench/run_simplewiki_openai_20k_100.sh`

### 20k / 100

- `pipnn` was still building after more than `3 minutes`
- this run was stopped and treated as a negative signal

### 5k / 50 Baseline

Artifacts:

- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_5k_50/hnsw_metrics.json`
- `remote-logs/high_dim_validation/simplewiki-openai-5k50_20260312T095046Z.log`

PiPNN:

- `build_sec = 39.0241`
- `recall_at_10 = 0.978`
- `qps = 36.912`

PiPNN build profile:

- `partition_sec = 18.4711`
- `leaf_knn_sec = 9.34212`
- `prune_sec = 11.2098`
- `leaves = 103`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 48.5198`
- `recall_at_10 = 0.998`
- `qps = 145.651`

Interpretation:

- High-dimensional data does not make the current PiPNN path naturally efficient.
- PiPNN is still slightly faster to build than HNSW on this small slice, but query speed is much worse and recall is lower.
- The main build bottleneck is no longer only `leaf_knn`; `partition` is already the largest phase on `3072d`.
- This points to a broader distance-computation problem across both RBC assignment and leaf candidate generation, not just a leaf-local kernel problem.

## RBC Partition Batching

Scope:

- change only the point-to-leader assignment kernel inside `RBC`
- keep the `Feature 20` tree structure and overlap semantics unchanged

Artifacts:

- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_5k_50/hnsw_metrics.json`
- `remote-logs/high_dim_validation/simplewiki-openai-5k50-rbc-batch_20260312T105053Z.log`

### 5k / 50 After RBC batching

PiPNN:

- `build_sec = 30.4769`
- `recall_at_10 = 0.978`
- `qps = 36.2215`

PiPNN build profile:

- `partition_sec = 12.5956`
- `leaf_knn_sec = 8.13263`
- `prune_sec = 9.74774`
- `leaves = 103`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 51.6921`
- `recall_at_10 = 0.998`
- `qps = 136.034`

### Delta vs baseline

PiPNN deltas:

- `build_sec: 39.0241 -> 30.4769` (`-8.5472s`, about `-21.9%`)
- `partition_sec: 18.4711 -> 12.5956` (`-5.8755s`, about `-31.8%`)
- `leaf_knn_sec: 9.34212 -> 8.13263`
- `prune_sec: 11.2098 -> 9.74774`
- `recall_at_10: 0.978 -> 0.978`

Interpretation:

- `RBC`-first batching is justified. The dominant hotspot dropped materially without any recall loss.
- The optimization did not change graph quality on this smoke slice.
- `partition` is still the largest stage, but it is no longer overwhelmingly dominant.
- The next iteration should continue into `leaf_kNN` batching, not jump yet to a broader shared distance layer.

## Exact leaf_kNN Batching

Scope:

- change only the uncapped exact full-scan leaf kernel
- keep `RBC`, `HashPrune`, and search semantics unchanged

Artifacts:

- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_5k_50/hnsw_metrics.json`
- `remote-logs/high_dim_validation/simplewiki-openai-5k50-leaf-batch_20260312T112623Z.log`

### 5k / 50 After exact leaf batching

PiPNN:

- `build_sec = 38.6283`
- `recall_at_10 = 0.978`
- `qps = 36.6683`

PiPNN build profile:

- `partition_sec = 12.6393`
- `leaf_knn_sec = 16.1486`
- `prune_sec = 9.83931`
- `leaves = 103`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 51.0015`
- `recall_at_10 = 0.998`
- `qps = 144.639`

### Delta vs post-RBC baseline

PiPNN deltas:

- `build_sec: 30.4769 -> 38.6283` (`+8.1514s`, about `+26.7%`)
- `partition_sec: 12.5956 -> 12.6393`
- `leaf_knn_sec: 8.13263 -> 16.1486` (`+8.01597s`, about `+98.6%`)
- `prune_sec: 9.74774 -> 9.83931`
- `recall_at_10: 0.978 -> 0.978`

Interpretation:

- This exact per-leaf blocked kernel is a negative result on the current high-dimensional workload.
- The regression is isolated almost entirely to `leaf_knn_sec`; `partition` and `prune` are effectively unchanged.
- The current per-leaf matrix materialization strategy is not paying for itself at these leaf sizes.
- The next step should not be another small per-leaf blocked-kernel tweak.
- The next design should question the architecture and move to a broader shared distance-batching layer or a multi-leaf batching strategy.

## Multi-Leaf leaf_kNN Batching

Scope:

- move the batching boundary up to `pipnn_builder.cpp`
- collect uncapped large leaves as jobs
- batch multiple leaves together while preserving exact leaf-local top-k semantics
- keep `RBC`, `HashPrune`, and search semantics unchanged

Artifacts:

- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_5k_50/hnsw_metrics.json`
- `remote-logs/high_dim_validation/simplewiki-openai-5k50-multi-leaf-batch_20260312T120534Z.log`

### 5k / 50 After multi-leaf batching

PiPNN:

- `build_sec = 66.6072`
- `recall_at_10 = 0.978`
- `qps = 36.4256`

PiPNN build profile:

- `partition_sec = 12.9174`
- `leaf_knn_sec = 42.5382`
- `prune_sec = 11.1506`
- `leaves = 103`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 50.3638`
- `recall_at_10 = 0.998`
- `qps = 146.764`

### Delta vs per-leaf blocked result

PiPNN deltas:

- `build_sec: 38.6283 -> 66.6072` (`+27.9789s`, about `+72.4%`)
- `partition_sec: 12.6393 -> 12.9174`
- `leaf_knn_sec: 16.1486 -> 42.5382` (`+26.3896s`, about `+163.4%`)
- `prune_sec: 9.83931 -> 11.1506`
- `recall_at_10: 0.978 -> 0.978`

### Delta vs post-RBC baseline

PiPNN deltas:

- `build_sec: 30.4769 -> 66.6072` (`+36.1303s`, about `+118.6%`)
- `partition_sec: 12.5956 -> 12.9174`
- `leaf_knn_sec: 8.13263 -> 42.5382` (`+34.40557s`, about `+423.1%`)
- `prune_sec: 9.74774 -> 11.1506`
- `recall_at_10: 0.978 -> 0.978`

Interpretation:

- This multi-leaf exact batching path is also a negative result on the current high-dimensional workload.
- The regression remains concentrated in `leaf_knn_sec`; moving the batching boundary up did not recover performance.
- The result is not promotion-ready and not even informative in the narrow sense defined by the spec, because it is worse than the already-failing per-leaf blocked result.
- The evidence now points away from exact leaf batching as a productive direction for this implementation.
- The next design should stop investing in exact leaf batching and move either to:
  - a broader shared distance-batching layer with a different cost structure
  - or a different candidate-generation strategy that avoids exact full-scan leaf work

## RBC-Overlap Shortlist Candidate Generation

Scope:

- replace leaf full-scan candidate generation with membership-based shortlist candidates
- keep `RBC` tree structure, overlap semantics, `HashPrune`, and search unchanged
- use deterministic shortlist cap: `candidate_cap = max_degree * 8`

Artifacts:

- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_5k_50/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_5k_50/hnsw_metrics.json`
- `results/high_dim_validation/simplewiki_openai_20k_100_shortlist/pipnn_metrics.json`
- `results/high_dim_validation/simplewiki_openai_20k_100_shortlist/pipnn.stdout`
- `results/high_dim_validation/simplewiki_openai_20k_100_shortlist/hnsw_metrics.json`

### 5k / 50 with shortlist

PiPNN:

- `build_sec = 32.5795`
- `recall_at_10 = 0.962`
- `qps = 73.19`

PiPNN build profile:

- `partition_sec = 11.128`
- `leaf_knn_sec = 16.231`
- `prune_sec = 5.21938`
- `candidate_edges = 120000`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 51.3718`
- `recall_at_10 = 0.998`
- `qps = 145.535`

Delta vs failed multi-leaf exact batching:

- `build_sec: 66.6072 -> 32.5795` (`-34.0277s`, about `-51.1%`)
- `leaf_knn_sec: 42.5382 -> 16.231` (`-26.3072s`, about `-61.8%`)
- `recall_at_10: 0.978 -> 0.962`

Delta vs post-RBC baseline:

- `build_sec: 30.4769 -> 32.5795` (`+2.1026s`, about `+6.9%`)
- `partition_sec: 12.5956 -> 11.128` (`-1.4676s`, about `-11.7%`)
- `leaf_knn_sec: 8.13263 -> 16.231` (`+8.09837s`, about `+99.6%`)
- `recall_at_10: 0.978 -> 0.962`

### 20k / 100 with shortlist follow-up

PiPNN:

- `build_sec = 245.748`
- `recall_at_10 = 0.939`
- `qps = 62.9043`

PiPNN build profile:

- `partition_sec = 158.869`
- `leaf_knn_sec = 65.9538`
- `prune_sec = 20.922`
- `candidate_edges = 480000`
- `rbc_max_membership = 2`

HNSW:

- `build_sec = 329.473`
- `recall_at_10 = 0.994`
- `qps = 115.212`

Interpretation:

- Shortlist candidate generation is clearly better than both exact leaf-batching variants.
- On `5k/50`, it removes the severe `leaf_knn` regression but still does not beat the post-RBC checkpoint.
- On `20k/100`, PiPNN still builds faster than HNSW, but recall drops to `0.939`, below the current phase threshold.
- At larger slice size, `partition` is again the dominant stage (`158.869s`), so candidate-path changes alone are not enough.
- This direction is informative but not promotion-ready. Next iteration should co-optimize:
  - shortlist quality (to recover recall), and
  - `RBC` partition scaling (to control the dominant stage at larger `N`).

## Shortlist Parameter Sweep (remote, quick follow-up)

Artifacts:

- `results/high_dim_validation/param_sweep_10k/pipnn_metrics_f2_10k.json`
- `results/high_dim_validation/param_sweep_10k/pipnn_metrics_f2_l16_10k.json`
- `results/high_dim_validation/param_sweep_10k/pipnn_metrics_f3_10k.json`
- `results/high_dim_validation/param_sweep_10k/pipnn_metrics_f2_l16_20k.json`

### 10k / 100 (PiPNN only)

Baseline shortlist (`fanout=2`, `leaf_k=12`):

- `build_sec = 98.2209`
- `recall_at_10 = 0.938`
- `qps = 67.1831`

Increase `leaf_k` (`fanout=2`, `leaf_k=16`):

- `build_sec = 101.647`
- `recall_at_10 = 0.953`
- `qps = 57.1248`

Increase fanout (`fanout=3`, `leaf_k=12`):

- `build_sec = 162.701`
- `recall_at_10 = 0.966`
- `qps = 65.3271`

### 20k / 100 (PiPNN only)

Increase `leaf_k` (`fanout=2`, `leaf_k=16`):

- `build_sec = 281.981`
- `recall_at_10 = 0.944`
- `qps = 54.1159`

Interpretation:

- `leaf_k` increase helps recall with moderate build cost at `10k`, but the gain weakens at `20k` and still misses `0.95`.
- `fanout=3` improves recall at `10k`, but build cost rises sharply; this is unlikely to be a good default for larger slices.
- Current shortlist quality still does not scale to `20k/100` with acceptable speed/recall balance under this parameter family.

## Shortlist Parameter Sweep (remote, 20k refinement)

Artifacts:

- `results/high_dim_validation/param_sweep_20k/pipnn_metrics_f2_l16_d32_20k.json`
- `results/high_dim_validation/param_sweep_20k/pipnn_metrics_f2_l16_d48_20k.json`
- `results/high_dim_validation/param_sweep_20k/pipnn_metrics_f2_l18_d32_20k.json`
- `results/high_dim_validation/param_sweep_20k/pipnn_metrics_f2_l20_d32_20k.json`

Configuration summary (`fanout=2` fixed):

- `leaf_k=16, max_degree=32`: `build=281.981`, `recall=0.944`, `qps=54.1159`
- `leaf_k=16, max_degree=48`: `build=258.047`, `recall=0.945`, `qps=52.8325`
- `leaf_k=18, max_degree=32`: `build=298.69`, `recall=0.947`, `qps=50.3125`
- `leaf_k=20, max_degree=32`: `build=287.254`, `recall=0.952`, `qps=47.7327`

Interpretation:

- Raising `max_degree` alone did not recover recall for this workload.
- Increasing `leaf_k` is the dominant recall lever under current shortlist path.
- The first configuration that passes the `>=0.95` recall threshold at `20k/100` is:
  - `fanout=2, leaf_k=20, max_degree=32`
- Tradeoff of this passing config vs current `leaf_k=16, max_degree=32` baseline:
  - `build_sec: 281.981 -> 287.254` (`+5.273`)
  - `recall_at_10: 0.944 -> 0.952` (`+0.008`)
  - `qps: 54.1159 -> 47.7327` (`-6.3832`)

## HNSW Reference (remote, 20k/100)

Artifacts:

- `results/high_dim_validation/param_sweep_20k/hnsw_metrics_20k_ref.json`
- `results/high_dim_validation/param_sweep_20k/pipnn_metrics_f2_l20_d32_20k.json`
- `scripts/bench/sweep_hnsw_20k_100.sh`

Reference command:

```bash
./build/pipnn \
  --mode hnsw --dataset sift1m --metric l2 \
  --base /data/work/datasets/crisp/simplewiki-openai/base.fvecs \
  --query /data/work/datasets/crisp/simplewiki-openai/query.fvecs \
  --max-base 20000 --max-query 100 \
  --output results/high_dim_validation/param_sweep_20k/hnsw_metrics_20k_ref.json
```

Observed metrics:

- HNSW (default `M=32`, `ef_construction=200`, `ef_search=auto`):
  - `build_sec = 359.943`
  - `recall_at_10 = 0.994`
  - `qps = 103.305`
- PiPNN (`fanout=2`, `leaf_k=20`, `max_degree=32`):
  - `build_sec = 287.254`
  - `recall_at_10 = 0.952`
  - `qps = 47.7327`

20k fairness snapshot (same dataset slice, same machine):

- Build: PiPNN is faster (`287.254s` vs `359.943s`, about `1.25x` faster)
- Recall: PiPNN is lower (`0.952` vs `0.994`, `-0.042`)
- Query speed: PiPNN is lower (`47.7327` vs `103.305`, about `0.46x`)

Next step for stricter fairness:

- Use `scripts/bench/sweep_hnsw_20k_100.sh` to generate recall-matched HNSW points under the same `20k/100` slice, then compare both methods at matched recall.

## HNSW Recall-Matched Sweep (remote, 20k/100)

Artifacts:

- `scripts/bench/run_hnsw_match_sweep_20k_100_v1.sh`
- `results/high_dim_validation/hnsw_sweep_20k_100_match_v1/summary.tsv`
- `results/high_dim_validation/hnsw_sweep_20k_100_match_v1/hnsw_m16_efc100_efs28.json`
- `results/high_dim_validation/hnsw_sweep_20k_100_match_v1/hnsw_m16_efc100_efs32.json`

Sweep result highlights:

- `m16, efc100, efs24`: `build=153.894`, `recall=0.944`, `qps=425.288`
- `m16, efc100, efs28`: `build=154.722`, `recall=0.956`, `qps=379.074`
- `m16, efc100, efs32`: `build=160.07`, `recall=0.965`, `qps=328.292`

Recall-matched comparison (PiPNN target: `recall=0.952`):

- PiPNN (`fanout=2, leaf_k=20, max_degree=32`):
  - `build_sec = 287.254`
  - `recall_at_10 = 0.952`
  - `qps = 47.7327`
- HNSW nearest point (`m16, efc100, efs28`):
  - `build_sec = 154.722`
  - `recall_at_10 = 0.956`
  - `qps = 379.074`

Interpretation:

- At nearly matched recall (`+0.004` on HNSW), HNSW is faster on both axes in this slice:
  - build: about `1.86x` faster (`287.254 / 154.722`)
  - query: about `7.94x` higher QPS (`379.074 / 47.7327`)
- The previous “PiPNN build faster” conclusion does not hold under this recall-matched HNSW tuning point on `20k/100`.
