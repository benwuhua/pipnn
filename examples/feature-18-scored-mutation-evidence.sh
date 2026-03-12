#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORKERS="${PIPNN_MULL_WORKERS:-4}"
REFRESH="${1:-validate}"

cd "${REPO_ROOT}"

if [[ "${REFRESH}" == "refresh" ]]; then
  bash scripts/quality/remote_mutation_run.sh --mode full --workers "${WORKERS}"
fi

python3 scripts/validate_mutation_evidence.py
python3 scripts/validate_repro_manifest.py results/repro_manifest.json
