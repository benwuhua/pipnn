# Feature Plan — Feature 1 CLI 参数与模式路由 Closure

- Date: 2026-03-12
- Feature: `#1 CLI 参数与模式路由`
- Scope: close the reopened CLI feature under wave-2 quality rules without changing observable CLI behavior, by adding the missing real-test harness asset, generating the missing acceptance artifacts, and re-verifying the existing interface end to end.

## Design Alignment

- Treat Feature 1 as the CLI surface described by `IFR-001` and the observable behavior required by `FR-003` and `FR-006`.
- Keep `./build/pipnn` as the only public interface and preserve the current routing split: CLI parsing in `src/cli/app.cpp`, process entry in `src/main.cpp`, and benchmark execution delegated to `RunBenchmark(...)`.
- Reuse the existing quality evidence workflow from design section `4.3` for coverage and mutation proof instead of introducing a feature-specific variant.

## Harness-First Work

- Add the missing `scripts/check_real_tests.py` script referenced by the long-task quality process and align `long-task-guide.md` with the actual marker convention used by the CLI integration tests.
- Add an automated regression test for the harness script so the repository enforces the workflow instead of relying on oral context.

## Tasks

1. Add a failing harness regression test for `scripts/check_real_tests.py`.
2. Implement `scripts/check_real_tests.py` to scan repo test files, count `[integration]` markers, and warn on `mock/stub/fake` keywords.
3. Update `long-task-guide.md` so the Real Test Convention matches the script and the existing CLI tests.
4. Add ST trace comments to the existing CLI tests so acceptance cases can be traced mechanically during review.
5. Create Feature 1 acceptance artifacts:
   - `docs/test-cases/feature-1-cli-parameter-routing.md`
   - `examples/feature-1-cli-parameter-routing.sh`
6. Re-run Feature 1 quality gates in order:
   - `python3 scripts/check_real_tests.py feature-list.json --feature 1`
   - `ctest --test-dir build --output-on-failure`
   - `bash scripts/quality/remote_coverage.sh`
   - `bash scripts/quality/remote_mutation_probe.sh`
7. Re-verify the Feature 1 CLI scenarios directly via the example and acceptance cases, then update status/progress files.

## Files

- `docs/plans/2026-03-12-feature-1-cli-closure.md`
- `long-task-guide.md`
- `scripts/check_real_tests.py`
- `tests/CMakeLists.txt`
- `tests/test_real_test_harness.cpp`
- `tests/test_cli.cpp`
- `tests/test_cli_app.cpp`
- `docs/test-cases/feature-1-cli-parameter-routing.md`
- `examples/feature-1-cli-parameter-routing.sh`
