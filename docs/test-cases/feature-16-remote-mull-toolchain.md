# Feature 16 Test Cases — Remote LLVM/Mull User-Space Toolchain

## Scope

Feature 16 proves that the remote x86 host can provision a repo-local LLVM + Mull toolchain with stable paths and pinned versions.

## Acceptance Cases

### TC-16-001 Remote toolchain smoke

- **Given** the generic x86 remote configuration is available and the repo can be synced to `/data/work/pipnn`
- **When** running `bash scripts/quality/remote_mull_toolchain_smoke.sh`
- **Then** the command succeeds
- **And** it fetches `results/st/remote/mull_toolchain_paths.txt`
- **And** the fetched report contains:
  - `clang=.tools/llvm/current/bin/clang`
  - `clangxx=.tools/llvm/current/bin/clang++`
  - `mull_runner=.tools/mull/current/bin/mull-runner`
  - `llvm_version=17.0.6`
  - `mull_version=0.29.0`
  - `mull_llvm_major=17`

### TC-16-002 Local fixture harness

- **Given** prepared fake LLVM and Mull source trees
- **When** running `ctest --test-dir build --output-on-failure -R '^remote_toolchain$'`
- **Then** the harness verifies:
  - stable repo-local paths are produced
  - wrapper binaries exist under `current/bin`
  - missing required LLVM executables fail deterministically

## Evidence

- Installer: `scripts/quality/ensure_remote_mull_toolchain.sh`
- Remote smoke: `scripts/quality/remote_mull_toolchain_smoke.sh`
- Runbook: `docs/runbooks/mutation-evidence.md`
- Example: `examples/feature-16-remote-mull-toolchain.sh`
