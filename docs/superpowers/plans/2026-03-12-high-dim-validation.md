# High-Dimension Validation Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one high-dimensional `L2` dataset path, validate PiPNN and HNSW on it, and determine whether the current leaf-building direction behaves better on a higher-dimensional workload than it does on `SIFT1M`.

**Architecture:** Introduce a small, separate binary reader module for the chosen high-dimensional dataset format rather than inflating `sift_reader.cpp`. Route a new dataset keyword through the existing CLI, keep metric support at `L2` only, and add a remote benchmark script that mirrors the existing result artifact shape so the new dataset can be compared directly with the `SIFT1M` understanding.

**Tech Stack:** C++20, CMake, ctest, repo-local dataset readers, generic-x86-remote scripts, JSON benchmark artifacts.

---

## File Map

- Create: `scripts/bench/probe_openai_arxiv_format.sh`
  - Repo-local remote probe wrapper that inspects the remote dataset directory and writes a small artifact describing files and binary headers.
- Create: `src/data/openai_reader.h`
  - Focused binary reader declarations for the chosen high-dimensional dataset format.
- Create: `src/data/openai_reader.cpp`
  - Reader implementation for the chosen base/query/truth binary files.
- Modify: `src/cli/app.cpp`
  - Add dataset route `openai-arxiv` and help text.
- Modify: `CMakeLists.txt`
  - Add the new reader source file to `pipnn_core`.
- Modify: `tests/test_sift_reader.cpp`
  - Either extend it carefully or split out a new dataset-reader test if format logic becomes too different.
- Create: `tests/test_openai_reader.cpp`
  - Dedicated tests for the new binary format reader.
- Modify: `tests/CMakeLists.txt`
  - Register the new reader test.
- Modify: `tests/test_cli_app.cpp`
  - Add CLI smoke coverage for the new dataset keyword and missing-file path.
- Create: `scripts/bench/run_openai_arxiv_100k_200.sh`
  - Remote benchmark wrapper for the high-dimensional validation slice.
- Modify: `task-progress.md`
  - Record data path, benchmark evidence, and the resulting algorithm decision.
- Modify: `RELEASE_NOTES.md`
  - Record the high-dimensional validation branch checkpoint.

## Chunk 1: Remote Format Probe

### Task 1: Add a repo-local remote probe for the dataset files

**Files:**
- Create: `scripts/bench/probe_openai_arxiv_format.sh`

- [ ] **Step 1: Write the probe wrapper**

Create a script that uses the existing `generic-x86-remote` tooling to inspect the remote dataset directory and emit a deterministic artifact under `results/`.

The script must capture:
- the resolved remote dataset path
- the file list under that dataset directory
- file sizes
- a small binary header dump for the first few bytes of base/query/truth files if present

- [ ] **Step 2: Verify the script parses as shell**

Run:

```bash
bash -n scripts/bench/probe_openai_arxiv_format.sh
```

Expected: no syntax errors.

- [ ] **Step 3: Run the probe and collect evidence**

Run:

```bash
bash scripts/bench/probe_openai_arxiv_format.sh
```

Expected artifact examples:
- `results/openai_arxiv_probe/files.txt`
- `results/openai_arxiv_probe/headers.txt`

If the remote path is wrong or polluted, stop and update the plan inputs before moving to reader code.

- [ ] **Step 4: Commit**

```bash
git add scripts/bench/probe_openai_arxiv_format.sh results/openai_arxiv_probe
git commit -m "chore: add openai-arxiv remote format probe"
```

## Chunk 2: Reader And CLI Route

### Task 2: Add a failing reader test for the chosen binary format

**Files:**
- Create: `tests/test_openai_reader.cpp`
- Test: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing reader test**

Based on the probe output, create a format-specific test fixture that writes minimal binary sample files for:
- base vectors
- query vectors
- optional truth rows

The test must assert:
- row count and dimension are correct
- truncated files throw a diagnostic error
- missing files throw a diagnostic error
- `max_rows` limits are honored

- [ ] **Step 2: Register the new test and run it to verify failure**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^openai_reader$'
```

Expected: failure because the reader does not exist yet.

- [ ] **Step 3: Commit**

```bash
git add tests/test_openai_reader.cpp tests/CMakeLists.txt
git commit -m "test: add openai-arxiv reader coverage"
```

### Task 3: Implement the new binary reader

**Files:**
- Create: `src/data/openai_reader.h`
- Create: `src/data/openai_reader.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_openai_reader.cpp`

- [ ] **Step 1: Write the minimal reader implementation**

Implement the binary reader matching the probed format. Keep it separate from `sift_reader.cpp`.

The implementation should:
- open the chosen files explicitly
- validate dimension metadata if present
- return `Matrix` / truth rows in the same shapes used by the rest of the code
- throw clear `std::runtime_error` messages for missing, truncated, or malformed files

- [ ] **Step 2: Add the new source file to `pipnn_core`**

Modify `CMakeLists.txt` so the reader is compiled into the main library.

- [ ] **Step 3: Run the targeted tests to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(openai_reader|sift_reader)$'
```

Expected: both tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/data/openai_reader.h src/data/openai_reader.cpp CMakeLists.txt
git commit -m "feat: add openai-arxiv binary reader"
```

### Task 4: Route the new dataset through the CLI

**Files:**
- Modify: `src/cli/app.cpp`
- Modify: `tests/test_cli_app.cpp`

- [ ] **Step 1: Add a failing CLI test**

Extend `tests/test_cli_app.cpp` with two new cases:
- `--dataset openai-arxiv` without `--base/--query` fails with a dataset-specific message
- `--dataset openai-arxiv` with nonexistent files fails with the new reader's missing-file diagnostic

- [ ] **Step 2: Run the CLI test to verify failure**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(cli_app)$'
```

Expected: failure because the dataset route is not implemented yet.

- [ ] **Step 3: Implement the dataset route in `app.cpp`**

Update:
- help text to mention `openai-arxiv`
- dataset dispatch to call the new reader
- dataset-specific argument validation, while preserving `synthetic` and `sift1m` unchanged

- [ ] **Step 4: Re-run targeted tests**

Run:

```bash
ctest --test-dir build --output-on-failure -R '^(openai_reader|cli_app|sift_reader)$'
```

Expected: all targeted tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/cli/app.cpp tests/test_cli_app.cpp
git commit -m "feat: add openai-arxiv cli route"
```

## Chunk 3: Smoke Validation

### Task 5: Add a repeatable remote benchmark wrapper

**Files:**
- Create: `scripts/bench/run_openai_arxiv_100k_200.sh`

- [ ] **Step 1: Write the remote wrapper**

The wrapper should:
- use the existing remote x86 workflow style
- point at the probed remote dataset files explicitly
- run both `pipnn` and `hnsw` on a fixed validation slice
- capture `pipnn_metrics.json`, `hnsw_metrics.json`, and stdout/profile artifacts

- [ ] **Step 2: Verify the script syntax**

Run:

```bash
bash -n scripts/bench/run_openai_arxiv_100k_200.sh
```

Expected: no shell syntax errors.

- [ ] **Step 3: Commit**

```bash
git add scripts/bench/run_openai_arxiv_100k_200.sh
git commit -m "bench: add openai-arxiv validation wrapper"
```

### Task 6: Run a small smoke benchmark on the new dataset

**Files:**
- Create or update: `results/openai_arxiv_smoke/`

- [ ] **Step 1: Run the smoke wrapper with a small slice**

Use a deliberately small slice first, for example `10k/100` or the nearest supported subset.

Run:

```bash
bash scripts/bench/run_openai_arxiv_100k_200.sh
```

or the same script with temporary env overrides for the smoke size.

- [ ] **Step 2: Verify that both modes produce metrics**

Check for:
- `pipnn_metrics.json`
- `hnsw_metrics.json`
- captured profile/stdout files

If either mode fails, stop and fix data plumbing before moving to algorithm comparison.

- [ ] **Step 3: Commit**

```bash
git add results/openai_arxiv_smoke
# plus any plumbing fixes if required
git commit -m "test: validate openai-arxiv smoke benchmark"
```

## Chunk 4: High-Dimension Validation Result

### Task 7: Run the high-dimensional validation slice and record the decision

**Files:**
- Modify: `task-progress.md`
- Modify: `RELEASE_NOTES.md`
- Create or update: `results/openai_arxiv_validation/`

- [ ] **Step 1: Run the validation benchmark**

Run the fixed validation slice on the high-dimensional dataset using the current wave-4 parameters.

Expected artifacts:
- `results/openai_arxiv_validation/pipnn_metrics.json`
- `results/openai_arxiv_validation/hnsw_metrics.json`
- profile/stdout captures

- [ ] **Step 2: Compare against the current `SIFT1M` understanding**

Extract and record:
- `build_sec`
- `recall_at_10`
- `qps`
- `partition_sec`
- `leaf_knn_sec`
- `prune_sec`

The result must support one explicit conclusion:
- `feature 21` looks more viable on higher-dimensional data, or
- the current leaf route still regresses and should be abandoned in favor of a different architecture

- [ ] **Step 3: Update progress docs**

Update:
- `task-progress.md`
- `RELEASE_NOTES.md`

Record the dataset path used, slice size, and the resulting algorithm decision.

- [ ] **Step 4: Run final local verification**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: local suite passes.

- [ ] **Step 5: Commit**

```bash
git add task-progress.md RELEASE_NOTES.md results/openai_arxiv_validation
# plus any remaining code/test changes if benchmark-driven fixes were needed
git commit -m "feat: add high-dim validation evidence"
```
