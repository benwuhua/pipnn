# Feature Plan — Feature 16 Remote LLVM/Mull User-Space Toolchain

- Date: 2026-03-12
- Feature: `#16 NFR-006 远端 LLVM/Mull 用户态工具链`
- Scope: add a repo-local remote installer for pinned `LLVM + Mull`, expose stable tool paths for later mutation features, and prove remote smoke readiness without changing the existing `build/` or `build-cov/` flows.

## Design Alignment

- Follow `NFR-006` and design section `4.4` by making the remote x86 host the only authoritative mutation environment.
- Keep the installer isolated from the regular benchmark and coverage paths; it should provision `.tools/llvm/` and `.tools/mull/` only.
- Feature 16 stops at toolchain readiness: no `build-mull/` orchestration or score aggregation yet.

## Harness-First Work

- Extend the existing mutation-evidence harness with a scored-state red test before writing installer code.
- Add a runbook and runnable example so future agents can reproduce the toolchain setup without oral context.

## Tasks

1. Extend `tests/test_mutation_evidence.cpp` with a scored-state fixture that currently fails because toolchain-backed scored evidence does not exist.
2. Implement `scripts/quality/ensure_remote_mull_toolchain.sh` with pinned `LLVM_VERSION` / `MULL_VERSION`, repo-local install directories, version verification, and a `--print-paths` mode.
3. Add `docs/runbooks/mutation-evidence.md` with install, verify, and failure-diagnosis commands.
4. Add `examples/feature-16-remote-mull-toolchain.sh` and `docs/test-cases/feature-16-remote-mull-toolchain.md` for smoke-level acceptance coverage.
5. Run local regression for `mutation_evidence`, then execute a remote smoke check that resolves `clang++` and `mull-runner` from `.tools/`.

## Files

- `tests/test_mutation_evidence.cpp`
- `scripts/quality/ensure_remote_mull_toolchain.sh`
- `docs/runbooks/mutation-evidence.md`
- `examples/feature-16-remote-mull-toolchain.sh`
- `docs/test-cases/feature-16-remote-mull-toolchain.md`
