# Mutation Evidence Runbook

## Purpose

Use this runbook when a feature cycle or system test needs the remote x86 LLVM + Mull toolchain for scored mutation evidence.

## Remote Toolchain Smoke

From the repository root:

```bash
bash scripts/quality/remote_mull_toolchain_smoke.sh
```

Expected output:

- `results/st/remote/mull_toolchain_paths.txt`

The fetched report must contain stable repo-local paths for:

- `clang`
- `clangxx`
- `llvm_config`
- `mull_runner`

It must also report the pinned versions:

- `llvm_version=17.0.6`
- `mull_version=0.29.0`
- `mull_llvm_major=17`

## Inner Installer

The remote smoke calls:

```bash
bash scripts/quality/ensure_remote_mull_toolchain.sh --print-paths
```

The installer writes into repo-local directories on the remote host:

- `.tools/llvm/current`
- `.tools/mull/current`
- `.tools/mull/current/compat/lib/x86_64-linux-gnu`

Pinned assets:

- LLVM: `clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz`
- Mull: `Mull-17-0.29.0-LLVM-17.0.6-ubuntu-amd64-24.04.deb`
- Compat library: `libclang-cpp17t64=1:17.0.6-9ubuntu1`

Important runtime note:

- `clang` / `clang++` come from the repo-local LLVM tarball
- `mull-runner` does **not** use the tarball's `libclang-cpp.so.17`
- the wrapper points Mull at the repo-local compat package so it matches Ubuntu's system `libLLVM-17.so.1`

## Local Fixture Mode

For fast harness tests, the installer accepts prepared source trees instead of downloading release assets:

```bash
PIPNN_LLVM_SOURCE_DIR=/tmp/fake-llvm \
PIPNN_MULL_SOURCE_DIR=/tmp/fake-mull \
PIPNN_TOOLCHAIN_ROOT=/tmp/pipnn-tools \
bash scripts/quality/ensure_remote_mull_toolchain.sh --print-paths
```

This mode is what `tests/test_remote_toolchain.cpp` uses.

## Failure Diagnosis

If the smoke fails:

1. Re-run the remote command directly to keep stderr visible:

```bash
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- \
  bash scripts/quality/ensure_remote_mull_toolchain.sh --print-paths
```

2. Verify the release assets are reachable:

```bash
curl -I https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/clang+llvm-17.0.6-x86_64-linux-gnu-ubuntu-22.04.tar.xz
curl -I https://github.com/mull-project/mull/releases/download/0.29.0/Mull-17-0.29.0-LLVM-17.0.6-ubuntu-amd64-24.04.deb
```

3. If `mull-runner` fails at runtime, inspect the installed tree on the remote host:

```bash
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run.sh --repo /data/work/pipnn -- \
  bash -lc 'ls -R .tools/llvm/current/bin .tools/mull/current/bin .tools/mull/current/lib | sed -n "1,120p"'
```
