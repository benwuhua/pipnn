# Feature 18 Plan — NFR-006 Scored Mutation Evidence

## Goal

Close feature 18 by replacing the legacy blocked-state evidence path with scored-state mutation evidence for the approved remote x86 increment set. The final state must produce `mutation_score.txt`, `mutation_report.json`, and `mutation_survivors.txt`, update the validator / ST report / reproducibility manifest, and satisfy the hard gate `mutation_score >= 80`.

## Inputs

- Feature: `18` — `NFR-006 scored mutation evidence`
- SRS: `docs/plans/2026-03-12-pipnn-poc-srs.md` `NFR-006`
- Design: `docs/plans/2026-03-12-pipnn-poc-design.md` §4.4 `远端 scored mutation pipeline`
- Wave-3 spec: `docs/superpowers/specs/2026-03-12-mutation-scoring-design.md`
- Wave-3 plan: `docs/superpowers/plans/2026-03-12-mutation-scoring.md`

## Current Evidence and Root Cause

1. Feature 17 already proves the remote pipeline is mechanically usable.
2. The current raw aggregate in `results/st/mutation/full_summary.json` is misleading for feature 18 because it naively sums the same mutant across multiple test executables.
3. A deduplicated pass over the existing raw reports yields:
   - unique mutants: `57`
   - killed: `38`
   - survived: `19`
   - score: `66.7`
4. Therefore feature 18 needs both:
   - a corrected dedup/global aggregation model
   - additional tests that kill at least `8` more surviving mutants

## Harness-First Work

Before any product-facing status change:

1. Make aggregation deterministic and auditable.
   - Extend `scripts/quality/aggregate_mutation_reports.py` to deduplicate by `(file, mutant id)` across report files.
   - Apply status precedence so a mutant is treated as `killed` if any executable kills it.
2. Make scored-state validation mechanical.
   - Extend `scripts/validate_mutation_evidence.py` and its tests to require scored artifacts, numeric score, tool versions, and report references.
3. Keep the repo as the only source of truth.
   - `results/repro_manifest.json`, `docs/plans/2026-03-12-st-report.md`, and example/test-case docs must all point to the same scored artifact paths.

## Implementation Tasks

### Task 1 — TDD red for scored-state aggregation and validation

Files:
- `tests/test_mutation_report_aggregator.cpp`
- `tests/test_mutation_evidence.cpp`

Work:
1. Add a failing aggregator fixture that repeats the same mutant id across two reports and expects dedup behavior with kill precedence.
2. Add a failing validator fixture that requires scored-state fields:
   - `mutation_score.txt`
   - `mutation_report.json`
   - `mutation_survivors.txt`
   - numeric score in manifest and ST report
3. Keep the existing blocked-state fixture coverage intact.

## Task 2 — Implement dedup aggregate outputs

Files:
- `scripts/quality/aggregate_mutation_reports.py`
- `scripts/quality/remote_mutation_run_inner.sh`

Work:
1. Change aggregation from per-report additive totals to global unique-mutant totals.
2. Emit the scored artifact set required by the design:
   - `results/st/mutation_score.txt`
   - `results/st/mutation_report.json`
   - `results/st/mutation_survivors.txt`
3. Keep raw per-executable JSON reports for traceability.
4. Ensure `remote_mutation_run_inner.sh` writes the stable artifact paths that feature 18 will validate.

## Task 3 — Kill the surviving mutants that currently block `>= 80`

Primary targets, based on the dedup survivor set:

1. `src/bench/runner.cpp`
   - add exact tests for recall calculation boundaries, empty truth rows, `k` truncation, and qps math
2. `src/core/hashprune.cpp`
   - add tests for hash-key computation, bucket replacement, and duplicate/threshold edge cases
3. `src/core/distance.h`
   - add exact multi-dimensional L2 assertions that fail on increment/subtraction mutations
4. `src/core/rbc.cpp`
   - add boundary tests for leader count / chunk partitioning math

Constraint:
- Do not expand or shrink the approved mutation scope; the increment set is fixed by wave 3.

## Task 4 — Run authoritative remote mutation and re-check score

Commands:
```bash
ctest --test-dir build --output-on-failure -R '^(mutation_report_aggregator|mutation_evidence|hashprune|leaf_knn|rbc|pipnn_integration|cli_app|cli)$'
bash scripts/quality/remote_mutation_run.sh
```

Expected:
- fresh scored artifacts exist under `results/st/`
- deduplicated mutation score is numeric and `>= 80`

If the score is still `< 80`, continue the TDD loop on the surviving mutant set before touching documentation.

## Task 5 — Update scored-state evidence and release documents

Files:
- `scripts/validate_mutation_evidence.py`
- `results/repro_manifest.json`
- `docs/plans/2026-03-12-st-report.md`
- `docs/test-cases/feature-18-scored-mutation-evidence.md`
- `examples/feature-18-scored-mutation-evidence.sh`

Work:
1. Switch the validator, repro manifest, and ST report from blocked-state wording to scored-state facts.
2. Record:
   - mutation command
   - tool versions
   - aggregate score
   - report paths
   - survivor artifact path
3. Make the example run the authoritative mutation command and validation path end-to-end.

## Verification

1. `ctest --test-dir build --output-on-failure`
2. `bash scripts/quality/remote_coverage.sh`
3. `bash scripts/quality/remote_mutation_run.sh`
4. `python3 scripts/validate_mutation_evidence.py`
5. `python3 scripts/validate_repro_manifest.py results/repro_manifest.json`
6. `python3 scripts/validate_features.py feature-list.json`
7. `python3 scripts/check_st_readiness.py feature-list.json`
8. `./examples/feature-18-scored-mutation-evidence.sh`
9. `git diff --check`

## Exit Criteria

- feature 18 verification steps are satisfied exactly as written
- mutation evidence is in scored-state, not blocked-state
- deduplicated aggregate score is `>= 80`
- all status/docs/example artifacts reference the same scored evidence bundle
