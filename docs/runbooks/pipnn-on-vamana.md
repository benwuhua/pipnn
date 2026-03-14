# PiPNN-on-Vamana Runbook

## Purpose

This runbook captures the current state of the `PiPNN-on-Vamana` convergence path and the upstream feasibility check for `DiskANN`.

## Current Mainline

The repository now has local seams for:

- PiPNN candidate generation
- Vamana graph refinement
- Vamana search adapter
- benchmark modes:
  - `vamana`
  - `pipnn_vamana`

These seams are local transition layers. They are not yet wired to upstream `DiskANN cpp_main`.

The repository also now has a generic file-backed dataset path:

- CLI dataset mode: `--dataset file`
- supported vector formats:
  - floats: `.fvecs`, `.fbin`
  - truth/int vectors: `.ivecs`, `.ibin`

This is the bridge needed to run `PiPNN-on-Vamana` against `wikipedia-cohere-1m` without hard-coding dataset-specific readers.

Metric note for `wikipedia-cohere-1m`:

- the official `gt.ibin` aligns with inner product, not `l2`
- for this dataset, `METRIC=ip` is the correct default for benchmark scripts

## Upstream Target

Preferred upstream base:

- `microsoft/DiskANN`
- branch: `cpp_main`

Reason:

- current project constraint is `C++ + CMake`
- this keeps the architecture migration separate from any Rust migration

## Remote Probe

Remote probe command:

```bash
bash scripts/bench/remote_probe_diskann_cpp_main.sh
```

What it does:

- clones `DiskANN` `cpp_main` with submodules on the remote x86 host
- runs a minimal `cmake -DCMAKE_BUILD_TYPE=Release ..` configure step

## Current Probe Result

Observed on the current remote x86 host:

- `git`, `cmake`, and `g++` are present
- `libaio-dev` and `Boost` are present
- `rustc` and `cargo` are also present
- `DiskANN cpp_main` configure currently fails before build because it requires Intel OpenMP:

```text
Could not find Intel OMP in standard locations; use -DOMP_PATH to specify the install location for your environment
```

Additional observed dependency state:

- `libgomp.so.1` is present
- `libiomp5.so` is absent
- `MKL` packages are absent

## Interpretation

This means:

- upstream `DiskANN cpp_main` cannot be used on the current remote host without provisioning at least:
  - Intel OpenMP (`libiomp5`)
  - Intel MKL

This is an environment blocker, not a repository code blocker.

## Recommended Next Step

Choose one path and keep the decision explicit:

- Path A: provision the remote x86 host with Intel OpenMP + MKL, then retry the upstream probe
- Path B: continue implementation against the local Vamana seams until a suitable `DiskANN cpp_main` environment is available
- Path C: run a separate Rust-based `DiskANN` spike, but keep it outside the current C++ mainline

## Full-Dataset Entry Point

For the current local-seam mainline, the full `wikipedia-cohere-1m` benchmark entry is:

```bash
bash scripts/bench/run_wikipedia_cohere_1m_full.sh
```

Environment knobs:

- `WIKIPEDIA_COHERE_DIR`
- `OUT_DIR`
- `MAX_BASE`
- `MAX_QUERY`
- `RBC_CMAX`
- `RBC_FANOUT`
- `LEADER_FRAC`
- `MAX_LEADERS`
- `REPLICAS`
- `LEAF_K`
- `LEAF_SCAN_CAP`
- `MAX_DEGREE`
- `HASH_BITS`
- `BEAM`
- `BIDIRECTED`

Default behavior:

- runs `ctest`
- benchmarks:
  - `pipnn_vamana`
  - `vamana`
- defaults to `METRIC=ip`
- uses full dataset when `MAX_BASE=0` and `MAX_QUERY=0`

## Authority Entry Point

For the current authority checkpoint, the reproducible `PiPNN-on-Vamana` command is:

```bash
bash scripts/bench/run_wikipedia_cohere_1m_100_pipnn_vamana.sh
```

Default behavior:

- runs `ctest`
- benchmarks:
  - `pipnn_vamana`
- defaults to `METRIC=ip`
- uses full base when `MAX_BASE=0`
- uses the first `100` official queries when `MAX_QUERY=100`

Why this entry exists separately:

- the current repository-local `vamana` seam still uses an exact native candidate build
- that seam is not suitable for `1M` scale, so the authority `PiPNN-on-Vamana` run needs a dedicated reproducible path

## Metric Validation

Root-cause probe result:

- exact `inner product` top-10 matches the official `gt.ibin`
- exact `l2` top-10 does not
- therefore earlier `recall_at_10=0` results on `wikipedia-cohere-1m` were primarily a metric mismatch, not by themselves proof that the graph path was broken

Remote smoke on `5k/10` with subset-internal exact truth and `METRIC=ip`:

- `pipnn_vamana`:
  - `build_sec=4.899`
  - `recall_at_10=0.96`
  - `qps=129.957`
  - `edges=88377`
- `hnsw`:
  - `build_sec=8.81902`
  - `recall_at_10=1.0`
  - `qps=555.846`
  - `edges=160000`

Interpretation:

- the `ip` metric plumbing is working on the current mainline
- `pipnn_vamana` now produces non-zero recall on real `wikipedia-cohere-1m` data when evaluated under the correct metric

## Current Authority Result

Completed remote authority rerun:

- repo: `/data/work/pipnn-ip-metric`
- scope: `1M/100`
- mode: `pipnn_vamana`
- metric: `ip`
- log: `/data/work/logs/wikipedia-cohere-pipnn-vamana-1m100-ip_20260314T024612Z.log`
- local fetched artifacts:
  - `results/wikipedia_cohere_1m_100_ip/pipnn_vamana_metrics.json`
  - `results/wikipedia_cohere_1m_100_ip/pipnn_vamana.stdout`
  - `results/wikipedia_cohere_1m_100_ip/proc_snapshot.txt`

Metrics:

- `build_sec=5963.18`
- `recall_at_10=0.802`
- `qps=43.9503`
- `edges=18274237`

Interpretation:

- this is the current valid authority checkpoint for `wikipedia-cohere-1m`
- it supersedes the older `l2`-based `1M/100` result, which was evaluated against the wrong truth ordering

## Long-Run Monitoring

For long-running benchmarks that do not stream progress to stdout, use:

```bash
bash scripts/bench/run_with_proc_monitor.sh \
  --snapshot results/wikipedia_cohere_1m_100/proc_snapshot.txt \
  --interval 30 \
  -- bash scripts/bench/run_wikipedia_cohere_1m_100_pipnn_vamana.sh
```

What it records:

- `ps` snapshot (`etime`, `%cpu`, `%mem`, `comm`)
- `/proc/<pid>/status` subset (`State`, `VmRSS`, `VmPeak`, `Threads`)
- `/proc/<pid>/io`

Why it exists:

- some `PiPNN-on-Vamana` long runs do not emit progress logs before final JSON flush
- this wrapper creates an auditable progress sidecar without changing the benchmarked binary
