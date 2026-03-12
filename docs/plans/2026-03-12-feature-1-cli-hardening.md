# Feature Plan — Feature 1 CLI 参数与模式路由

- Date: 2026-03-12
- Feature: `#1 CLI 参数与模式路由`
- Scope: keep CLI behavior aligned with `FR-003`, `FR-006`, and `IFR-001` while removing abort-style exits for loader failures and adding direct test coverage for the executable entry flow.

## Design Alignment

- Reuse the existing CLI interface from `docs/plans/2026-03-12-pipnn-poc-design.md` section `4.2` and keep `./build/pipnn` as the only public entrypoint.
- Keep benchmark execution delegated to `RunBenchmark(...)`; the CLI layer should only parse arguments, load datasets, and serialize results.
- Preserve the current JSON schema and mode routing semantics for `pipnn` and `hnsw`.

## Tasks

1. Add CLI-level tests that exercise:
   - `--help`
   - `--mode hnsw --dataset synthetic`
   - missing SIFT input path handling with a controlled non-zero exit
2. Refactor `src/main.cpp` so the CLI logic is callable from tests without shelling out.
3. Add a top-level exception boundary that converts loader/runtime failures into diagnosable stderr output and a stable non-zero exit code.
4. Rebuild and rerun regression plus coverage to verify that `main.cpp` and HNSW mode are now exercised.

## Files

- `src/main.cpp`
- `tests/test_cli.cpp`
- `tests/CMakeLists.txt`
