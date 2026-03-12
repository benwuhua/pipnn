# Feature 1 Acceptance Test Cases — CLI 参数与模式路由

## Scope

Validate the observable CLI surface exposed by `./build/pipnn`, including mode routing, input validation, failure diagnostics, and basename output handling.

## Preconditions

- `cmake -S . -B build && cmake --build build -j` has completed successfully
- the repository root is the current working directory
- `./build/pipnn` exists

## Test Cases

### TC-1-001 Help Surface
- **Case ID**: `ST-FUNC-001-001`
- **Given** the `pipnn` binary exists
- **When** running `./build/pipnn --help`
- **Then** the command exits `0`
- **And** stdout contains `Usage: pipnn`
- **And** stdout contains `--mode <pipnn|hnsw>`

### TC-1-002 HNSW Synthetic Route
- **Case ID**: `ST-FUNC-001-002`
- **Given** a writable output path
- **When** running `./build/pipnn --mode hnsw --dataset synthetic --metric l2 --output <tmp>/hnsw.json`
- **Then** the command exits `0`
- **And** the JSON file contains `"mode": "hnsw"`

### TC-1-003 Missing SIFT File Diagnostic
- **Case ID**: `ST-FUNC-001-003`
- **Given** non-existent `--base` and `--query` paths
- **When** running `./build/pipnn --mode pipnn --dataset sift1m --metric l2 --base <missing> --query <missing> --output <tmp>/missing.json`
- **Then** the command exits non-zero
- **And** stderr contains `cannot open`

### TC-1-004 Unsupported Dataset
- **Case ID**: `ST-BNDRY-001-001`
- **Given** an unsupported dataset value
- **When** running `./build/pipnn --mode pipnn --dataset nope --metric l2 --output <tmp>/bad.json`
- **Then** the command exits non-zero
- **And** stderr contains `unsupported dataset: nope`

### TC-1-005 Invalid Numeric Option
- **Case ID**: `ST-BNDRY-001-002`
- **Given** a non-numeric `--beam` value
- **When** running `./build/pipnn --mode pipnn --dataset synthetic --metric l2 --output <tmp>/bad-beam.json --beam nope`
- **Then** the command exits non-zero
- **And** stderr contains `invalid value for --beam: nope`

### TC-1-006 Basename Output File
- **Case ID**: `ST-BNDRY-001-003`
- **Given** the current working directory is writable
- **When** changing into a temp directory and running `./build/pipnn --mode pipnn --dataset synthetic --metric l2 --output metrics.json`
- **Then** the command exits `0`
- **And** the current working directory now contains `metrics.json`

## Traceability

| Case ID | Verification Step / Requirement | Automated Coverage |
| --- | --- | --- |
| `ST-FUNC-001-001` | Feature 1 verification step 1 | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |
| `ST-FUNC-001-002` | `FR-003`, `IFR-001` | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |
| `ST-FUNC-001-003` | `FR-006` | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |
| `ST-BNDRY-001-001` | Feature 1 verification step 2 | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |
| `ST-BNDRY-001-002` | Feature 1 verification step 3 | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |
| `ST-BNDRY-001-003` | Feature 1 verification step 4 | `tests/test_cli.cpp`, `tests/test_cli_app.cpp` |

## Evidence

- Integration test target: `ctest --test-dir build --output-on-failure -R '^cli$'`
- Unit-level CLI target: `ctest --test-dir build --output-on-failure -R '^cli_app$'`
- Real-test marker check: `python3 scripts/check_real_tests.py feature-list.json --feature 1`
