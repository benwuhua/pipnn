# Feature 13 Plan: Reproducibility Manifest

- Date: 2026-03-12
- Feature: #13 `NFR-003 复现性`
- Target: make one complete remote evaluation auditable from a single local artifact.

## Requirement

- Commands used for remote evaluation must be recorded
- Remote log paths must be recorded and fetched locally
- Local result files must be linked to the commands and logs that produced them
- Key metrics must be readable without opening multiple files manually

## Recommended Approach

Use one checked-in manifest as the single audit entry point.

Manifest fields:
- environment commands: `check-env`, `sync`, `build`, `test`, `fetch`
- remote dataset path
- remote repo path
- one run entry per tracked benchmark
- local and remote log paths
- local and remote result paths
- key metrics copied from the result JSON

## Validation

Add a small validator that checks:
- manifest schema is present
- referenced local files exist
- recorded metrics match the referenced local result JSON

## Feature Scope

Tracked runs for this PoC:
- feature 11 `500k/100` PiPNN baseline (`beam=256`)
- feature 11 `500k/100` HNSW baseline
- feature 12 final quality config at `100k/100`, `200k/100`, `500k/100` (`beam=384`)

## Outputs

- `results/repro_manifest.json`
- `scripts/validate_repro_manifest.py`
- `docs/test-cases/feature-13-reproducibility.md`
- `examples/feature-13-reproducibility.sh`
