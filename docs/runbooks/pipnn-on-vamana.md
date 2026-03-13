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
- uses full dataset when `MAX_BASE=0` and `MAX_QUERY=0`
