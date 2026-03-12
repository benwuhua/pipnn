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
