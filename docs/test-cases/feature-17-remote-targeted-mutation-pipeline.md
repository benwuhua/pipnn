# Feature 17 Test Cases — Remote Targeted Mutation Pipeline

## Scope

Feature 17 proves that the remote x86 host can build a dedicated `build-mull/`
tree, run the approved targeted Mull executable set, and fetch raw
mutation-testing-elements reports into `results/st/mutation/`.

## Acceptance Cases

### TC-17-001 Remote smoke mutation run

- **Given** the repo-local LLVM/Mull toolchain is provisioned on the remote x86 host
- **When** running `bash scripts/quality/remote_mutation_run.sh --mode smoke --workers 1`
- **Then** the command succeeds
- **And** it fetches `results/st/mutation/smoke/`
- **And** the fetched directory contains:
  - `run_manifest.txt`
  - `test_hashprune.json`
  - `test_hashprune.log`

### TC-17-002 Remote full targeted mutation run

- **Given** the remote x86 host can provision the repo-local compat build compiler
- **When** running `bash scripts/quality/remote_mutation_run.sh --mode full --workers 1`
- **Then** the command succeeds
- **And** it fetches `results/st/mutation/full/`
- **And** the fetched directory contains a raw JSON report and log for each target:
  - `test_cli_app`
  - `test_hashprune`
  - `test_leaf_knn`
  - `test_rbc`
  - `test_pipnn_integration`

### TC-17-003 Local aggregation harness

- **Given** synthetic Elements-style raw JSON fixtures
- **When** running `ctest --test-dir build --output-on-failure -R '^mutation_report_aggregator$'`
- **Then** the harness verifies:
  - aggregate mutant counts
  - killed vs survived counts
  - mutation score calculation
  - invalid report shape failure

## Evidence

- Outer wrapper: `scripts/quality/remote_mutation_run.sh`
- Inner runner: `scripts/quality/remote_mutation_run_inner.sh`
- Build compiler helper: `scripts/quality/ensure_remote_mull_build_compiler.sh`
- Raw aggregator harness: `scripts/quality/aggregate_mutation_reports.py`
- Example: `examples/feature-17-remote-targeted-mutation-pipeline.sh`
