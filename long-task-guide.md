# Long Task Worker Guide — PiPNN PoC

## Orient
- Read `task-progress.md` `## Current State` and latest session log.
- Open `feature-list.json`, pick the first `status=failing` feature whose dependencies are all `passing`.
- Confirm required configs with Config Gate before coding.

## Bootstrap
- Build once:
```bash
cmake -S . -B build
cmake --build build -j
```
- Run baseline tests:
```bash
ctest --test-dir build --output-on-failure
```

## Config Gate
- Check required configs globally:
```bash
python3 scripts/check_configs.py feature-list.json
```
- Check required configs for one feature:
```bash
python3 scripts/check_configs.py feature-list.json --feature <id>
```

## TDD Red
- Add or update test first in `tests/`.
- Ensure new/updated test fails for the intended reason.

## TDD Green
- Implement minimal change in `src/`.
- Re-run targeted test, then full suite.

## Coverage Gate
- Run coverage after green:
```bash
gcovr -r . --txt
```
- Thresholds are in `feature-list.json` `quality_gates`.

## TDD Refactor
- Refactor only after green.
- Keep behavior unchanged and tests green.

## Mutation Gate
- Run mutation tooling check/execute when available:
```bash
mull-runner --help
```
- For full mutation campaign, configure mull test command and target files.

## Verification Enforcement
- No completion claim without fresh command evidence.
- Always include command + key output in session notes.

## Code Review
- Focus: correctness regressions, boundary cases, missing tests.
- If no issue found, explicitly state residual risks.

## Environment Commands
- Activation (if python helper env used):
```bash
source .venv/bin/activate
```
- Direct test execution:
```bash
ctest --test-dir build --output-on-failure
```
- Direct mutation tooling command:
```bash
mull-runner --help
```
- Direct coverage command:
```bash
gcovr -r . --txt
```

## Config Management
- Remote and dataset configs are managed by:
  - `~/.config/generic-x86-remote/remote.env`
  - environment variable `SIFT1M_DIR`
- To add a new config:
  1. Add entry to `feature-list.json` `required_configs`
  2. Add variable template to `.env.example` if env-type
  3. Update `scripts/check_configs.py` logic if needed

## Real Test Convention
- Identification: files matching `tests/test_*.cpp`.
- Real test command:
```bash
ctest --test-dir build --output-on-failure
```
- Example real test flow:
  - Build project
  - Run `ctest`
  - Confirm expected fail/pass based on TDD stage

## Examples
- Run synthetic benchmark:
```bash
./build/pipnn --mode pipnn --dataset synthetic --metric l2 --output results/pipnn_metrics.json
```
- Run remote benchmark:
```bash
/Users/ryan/.codex/skills/generic-x86-remote/scripts/run-bg.sh --repo /data/work/pipnn --name pipnn-sift1m -- ./scripts/bench/remote_bench_sift1m.sh
```

## Persist
- Update `feature-list.json` statuses after each completed feature.
- Append session entry to `task-progress.md`.
- Update `RELEASE_NOTES.md` for user-visible changes.

## Critical Rules
- Never skip TDD red-green checks.
- Never claim completion without command evidence.
- Keep features independently verifiable and dependency-aware.
