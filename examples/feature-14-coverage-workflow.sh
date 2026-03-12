#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

bash scripts/quality/remote_coverage.sh
python3 scripts/validate_quality_evidence.py
