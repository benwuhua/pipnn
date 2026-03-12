# Task Progress — pipnn-poc

## Current State
Progress: 12/13 passing · Last: System Testing complete (2026-03-12, Conditional-Go) · Next: Feature 1 CLI 参数与模式路由 quality debt follow-up

---

## Session Log


### Session 0 — 2026-03-12
- Initialized long-task artifacts from SRS + Design.
- Populated feature-list.json with FR/NFR decomposition, dependencies, and required configs.
- Added project scripts: validate_features.py, check_configs.py, get_tool_commands.py, check_st_readiness.py.
- Generated long-task-guide.md, env-guide.md, init.sh, init.ps1, .env.example.
- SRS reference: docs/plans/2026-03-12-pipnn-poc-srs.md
- Design reference: docs/plans/2026-03-12-pipnn-poc-design.md

### Session 1 — 2026-03-12
- Entered `long-task-work` for feature 11 (`100k/100、200k/100、500k/100 口径评测`).
- Revalidated config gate with `SIFT1M_DIR=/data/work/knowhere-rs-src/data/sift`.
- Synced the repository to remote path `/data/work/pipnn` and reran remote build + `ctest` successfully.
- Wrote feature plan: `docs/plans/2026-03-12-feature-11-benchmark-matrix.md`.
- Identified a methodology issue: `scripts/bench/remote_bench_sift1m.sh` passes full `groundtruth.ivecs`, which is not valid for subset-quality evaluation with `--max-base`.
- Re-ran `500k/100` PiPNN using direct binary invocation without `--truth` (subset-internal truth).
- Captured PiPNN `500k/100` result: `build_sec=401.906`, `recall_at_10=0.943`, `qps=219.942`, `edges=13219687`.
- Captured PiPNN profile: `partition_sec=110.784`, `leaf_knn_sec=245.780`, `prune_sec=45.213`.
- Started HNSW `500k/100` subset-truth baseline under remote log `remote-logs/hnsw-500k100-subset_20260312T010152Z.log`; result pending.
- Updated `results/pipnn_vs_hnsw_sift1m.md` with the 500k partial finding and remote command corrections.

### Session 2 — 2026-03-12
- Fetched the completed HNSW `500k/100` subset-truth result: `build_sec=1031.69`, `recall_at_10=0.988`, `qps=983.734`, `edges=16000000`.
- Finalized the `500k/100` section in `results/pipnn_vs_hnsw_sift1m.md`, including PiPNN and HNSW log paths.
- Added feature acceptance document: `docs/test-cases/feature-11-benchmark-matrix.md`.
- Added runnable example: `examples/feature-11-benchmark-matrix.sh`.
- Marked feature 11 as `passing`; feature 12 remains the next active item because `500k/100` PiPNN recall is still `0.943 < 0.95`.

### Session 3 — 2026-03-12
- Started feature 12 (`NFR-002 召回阈值达标`) from the known failing point: `500k/100` with `beam=256` gave `recall_at_10=0.943`.
- Formed and tested the search-budget hypothesis by increasing `beam` only, keeping graph-build parameters unchanged.
- Verified the unified `beam=384` configuration at all required scales:
  - `100k/100`: `recall_at_10=0.993`
  - `200k/100`: `recall_at_10=0.978`
  - `500k/100`: `recall_at_10=0.962`
- Stored evidence in `results/feature12_*_beam384.json` and corresponding `remote-logs/feature12-*-beam384_*.log`.
- Updated the benchmark report with the final feature 12 quality config and evidence log paths.
- Added feature acceptance document: `docs/test-cases/feature-12-recall-threshold.md`.
- Added runnable example: `examples/feature-12-recall-threshold.sh`.
- Marked feature 12 as `passing`; feature 13 is now the next active item.

### Session 4 — 2026-03-12
- Started feature 13 (`NFR-003 复现性`) from the requirement gap that commands, logs, and result files were documented but not unified under one audit entry point.
- Added `results/repro_manifest.json` as the single reproducibility manifest for the tracked benchmark runs.
- Added `scripts/validate_repro_manifest.py` to verify manifest structure, local log existence, local result existence, and metric consistency.
- Added feature acceptance document: `docs/test-cases/feature-13-reproducibility.md`.
- Added runnable example: `examples/feature-13-reproducibility.sh`.
- Updated the benchmark report to point to the reproducibility bundle and validator command.
- Marked feature 13 as `passing`; all active features are now passing and the project is ready for system testing.

### Session 5 — 2026-03-12
- Entered `long-task-st` after `python3 scripts/check_st_readiness.py feature-list.json` returned `READY`.
- Wrote system test plan: `docs/plans/2026-03-12-st-plan.md`.
- Executed local regression (`ctest`), remote x86 readiness (`check-env.sh --json`), remote x86 `ctest`, remote synthetic PiPNN smoke, local synthetic PiPNN/HNSW E2E runs, manifest validation, exploratory CLI error-path checks, and lightweight security grep review.
- Built a dedicated coverage configuration in `build-cov/` and reran `ctest` there.
- Measured line coverage with `gcovr`: `64%` against a `90%` threshold.
- Measured branch coverage with `gcovr`: `29%` against an `80%` threshold.
- Observed that mutation tooling is unavailable locally (`mull-runner: command not found`), so mutation score was not revalidated during ST.
- Wrote system test report: `docs/plans/2026-03-12-st-report.md`.
- Cleanup result: no services were started because the project is CLI-only; no service cleanup required.

### Session 6 — 2026-03-12
- Reopened feature 1 (`CLI 参数与模式路由`) to address the exploratory abort-on-missing-file defect and the zero-coverage `main.cpp`/HNSW CLI path.
- Wrote feature plan: `docs/plans/2026-03-12-feature-1-cli-hardening.md`.
- Added CLI integration coverage for `--help`, invalid metric, unsupported dataset, missing SIFT query, missing files, HNSW synthetic routing, tiny-SIFT PiPNN config/profile flow, and tiny-SIFT HNSW truth flow.
- Added direct HNSW baseline tests for empty input, truth-backed recall, and no-truth exact recall.
- Expanded `sift_reader` tests to cover `max_rows`, ivecs loading, missing files, inconsistent dims, truncated records, and non-positive dims.
- Wrapped `main()` with a top-level `std::exception` boundary so loader/runtime failures return exit code `1` instead of aborting.
- Updated the worker coverage command to use `build-cov` and exclude third-party `_deps` artifacts.
- Current source-only coverage after the new tests: line `95%`, branch `58%`; branch coverage and mutation tooling remain the blocking quality debts.
