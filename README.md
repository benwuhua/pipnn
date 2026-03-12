# PiPNN PoC

## Local

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/pipnn --help
./build/pipnn --mode pipnn --dataset synthetic --metric l2 --output results/pipnn_metrics.json
```

## Remote x86 (generic-x86-remote)

```bash
# Reuse knowhere remote settings
./scripts/bench/reuse_knowhere_remote_env.sh

/Users/ryan/.codex/skills/generic-x86-remote/scripts/check-env.sh --json
/Users/ryan/.codex/skills/generic-x86-remote/scripts/sync.sh --src /Users/ryan/Code/Paper/pipnn --dest /data/work/pipnn --delete --exclude .git --exclude build --exclude results
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run-bg.sh --repo /data/work/pipnn --name pipnn-sift1m -- ./scripts/bench/remote_bench_sift1m.sh
```

Expected in check output: `arch=x86_64`.

SIFT1M files expected under `$SIFT1M_DIR`:
- `base.fvecs`
- `query.fvecs`
- `groundtruth.ivecs`
