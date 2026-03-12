#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REMOTE_TOOLS_DIR="/Users/ryan/.codex/skills/generic-x86-remote/scripts"
REMOTE_REPO="${REMOTE_REPO_DIR:-/data/work/pipnn}"

"${REMOTE_TOOLS_DIR}/sync.sh" \
  --src "${REPO_ROOT}" \
  --dest "${REMOTE_REPO}" \
  --delete \
  --exclude .git \
  --exclude build \
  --exclude build-cov \
  --exclude results/st \
  --exclude remote-logs \
  --exclude '*.gcov'

"${REMOTE_TOOLS_DIR}/run.sh" --repo "${REMOTE_REPO}" -- \
  bash scripts/quality/remote_coverage_inner.sh

mkdir -p "${REPO_ROOT}/results/st"
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src "${REMOTE_REPO}/results/st" \
  --dest "${REPO_ROOT}/results/st"

echo "coverage=ok"
echo "local_results=${REPO_ROOT}/results/st"
