#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REMOTE_TOOLS_DIR="/Users/ryan/.codex/skills/generic-x86-remote/scripts"
REMOTE_REPO="${REMOTE_REPO_DIR:-/data/work/pipnn}"

MODE="full"
WORKERS="${PIPNN_MULL_WORKERS:-1}"

usage() {
  cat <<'EOF'
Usage: bash scripts/quality/remote_mutation_run.sh [--mode smoke|full] [--workers N]

Syncs the repo to the remote x86 host, runs the remote mutation pipeline, and
fetches results/st/mutation/ back into the local repository.
EOF
}

while (($#)); do
  case "$1" in
    --mode)
      MODE="$2"
      shift 2
      ;;
    --workers)
      WORKERS="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

"${REMOTE_TOOLS_DIR}/sync.sh" \
  --src "${REPO_ROOT}" \
  --dest "${REMOTE_REPO}" \
  --delete \
  --exclude .git \
  --exclude build \
  --exclude build-cov \
  --exclude build-mull \
  --exclude .tools \
  --exclude results/st \
  --exclude remote-logs \
  --exclude .worktrees \
  --exclude '*.gcov'

"${REMOTE_TOOLS_DIR}/run.sh" --repo "${REMOTE_REPO}" -- \
  bash scripts/quality/remote_mutation_run_inner.sh --mode "${MODE}" --workers "${WORKERS}"

mkdir -p "${REPO_ROOT}/results/st"
"${REMOTE_TOOLS_DIR}/fetch.sh" \
  --src "${REMOTE_REPO}/results/st" \
  --dest "${REPO_ROOT}/results/st"

echo "mutation=ok"
echo "mode=${MODE}"
echo "local_report_dir=${REPO_ROOT}/results/st/mutation/${MODE}"
