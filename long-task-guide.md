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
bash scripts/quality/remote_coverage.sh
```
- Thresholds are in `feature-list.json` `quality_gates`.
- Authoritative release evidence is collected on remote x86 GCC with the same source-only filters; local coverage is advisory unless the current feature explicitly states otherwise.

## TDD Refactor
- Refactor only after green.
- Keep behavior unchanged and tests green.

## Mutation Gate
- Run mutation tooling check/execute when available:
```bash
bash scripts/quality/remote_mutation_run.sh --mode full --workers 4
```
- This is the authoritative scored-state command for the approved remote x86 increment set.
- Legacy blocked-state evidence is only for pre-wave-3 revisions that do not ship the scored pipeline.

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
bash scripts/quality/remote_mutation_run.sh --mode full --workers 4
```
- Direct coverage command:
```bash
bash scripts/quality/remote_coverage.sh
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
- Identification:
  - discover candidate files via `feature-list.json` `real_test.marker_pattern`
  - treat blocks annotated with `[integration]` as real tests
  - treat blocks annotated with `[unit]` as non-real tests
- Real test command:
```bash
python3 scripts/check_real_tests.py feature-list.json --feature <id>
ctest --test-dir build --output-on-failure -R '^cli$'
```
- Example real test flow:
  - Build project
  - Run `python3 scripts/check_real_tests.py feature-list.json --feature <id>`
  - Run the feature's `[integration]` ctest target in isolation
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
