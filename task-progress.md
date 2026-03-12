# Task Progress — pipnn-poc

## Current State
Progress: 10/13 passing · Last: Feature 10 complete · Next: Feature 11 (500k/100 benchmark matrix)

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
