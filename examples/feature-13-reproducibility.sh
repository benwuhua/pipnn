#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

python3 "${ROOT}/scripts/validate_repro_manifest.py" "${ROOT}/results/repro_manifest.json"

python3 - "${ROOT}/results/repro_manifest.json" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1]))
print("Feature 13 example: reproducibility manifest summary")
print(f"dataset_root={data['dataset_root']}")
print(f"remote_repo={data['remote_repo']}")
print(f"tracked_runs={len(data['runs'])}")
for run in data["runs"]:
    print(
        f"{run['id']}: scale={run['scale']} mode={run['mode']} "
        f"log={run['local_log']} result={run['local_result']}"
    )
PY
