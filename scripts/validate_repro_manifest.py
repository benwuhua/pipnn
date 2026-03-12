#!/usr/bin/env python3
import json
import math
import sys
from pathlib import Path


REQUIRED_COMMANDS = {"check_env", "sync_repo", "build_repo", "test_repo", "fetch_logs"}
REQUIRED_RUN_KEYS = {
    "id",
    "purpose",
    "mode",
    "scale",
    "command",
    "remote_log",
    "local_log",
    "remote_result",
    "local_result",
    "metrics",
}
REQUIRED_METRICS = {"mode", "build_sec", "recall_at_10", "qps", "edges"}


def approx_equal(lhs, rhs) -> bool:
    if isinstance(lhs, float) or isinstance(rhs, float):
        return math.isclose(float(lhs), float(rhs), rel_tol=1e-9, abs_tol=1e-9)
    return lhs == rhs


def fail(msg: str) -> None:
    print(f"ERROR: {msg}")
    sys.exit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: validate_repro_manifest.py <manifest.json>")

    manifest_path = Path(sys.argv[1])
    if not manifest_path.exists():
        fail(f"missing manifest: {manifest_path}")

    data = json.loads(manifest_path.read_text())

    for key in ("generated_on", "local_repo", "remote_repo", "dataset_root", "commands", "runs"):
        if key not in data:
            fail(f"missing top-level key: {key}")

    commands = data["commands"]
    missing_commands = REQUIRED_COMMANDS - set(commands)
    if missing_commands:
        fail(f"missing command entries: {sorted(missing_commands)}")
    for name in REQUIRED_COMMANDS:
        if not isinstance(commands[name], str) or not commands[name].strip():
            fail(f"empty command entry: {name}")

    root = manifest_path.parent.parent
    for run in data["runs"]:
        missing = REQUIRED_RUN_KEYS - set(run)
        if missing:
            fail(f"run {run.get('id', '<unknown>')} missing keys: {sorted(missing)}")

        local_log = root / run["local_log"]
        local_result = root / run["local_result"]
        if not local_log.exists():
            fail(f"missing local log: {local_log}")
        if not local_result.exists():
            fail(f"missing local result: {local_result}")

        result = json.loads(local_result.read_text())
        metrics = run["metrics"]
        missing_metrics = REQUIRED_METRICS - set(metrics)
        if missing_metrics:
            fail(f"run {run['id']} missing metric keys: {sorted(missing_metrics)}")

        for key in REQUIRED_METRICS:
            if key not in result:
                fail(f"result file {local_result} missing metric: {key}")
            if not approx_equal(metrics[key], result[key]):
                fail(
                    f"metric mismatch for run {run['id']} key {key}: "
                    f"manifest={metrics[key]} result={result[key]}"
                )

    print(f"OK: validated {len(data['runs'])} reproducibility runs")


if __name__ == "__main__":
    main()
