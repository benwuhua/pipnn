#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:-smoke}"
WORKERS="${PIPNN_MULL_WORKERS:-1}"

cd "${REPO_ROOT}"
bash scripts/quality/remote_mutation_run.sh --mode "${MODE}" --workers "${WORKERS}"
