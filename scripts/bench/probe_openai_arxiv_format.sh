#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REMOTE_TOOLS_DIR="/Users/ryan/.codex/skills/generic-x86-remote/scripts"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/results/openai_arxiv_probe}"
REMOTE_PROBE_DIR="${REMOTE_PROBE_DIR:-/data/work/openai-arxiv-probe}"

mkdir -p "${OUT_DIR}"
PAYLOAD_DIR="$(mktemp -d "${OUT_DIR}/payload.XXXXXX")"
trap 'rm -rf "${PAYLOAD_DIR}"' EXIT
cp "${SCRIPT_DIR}/probe_openai_arxiv_format_remote.py" "${PAYLOAD_DIR}/"

"${REMOTE_TOOLS_DIR}/sync.sh" \
  --src "${PAYLOAD_DIR}" \
  --dest "${REMOTE_PROBE_DIR}" \
  --delete \
  --exclude .unused \
  > "${OUT_DIR}/sync.txt"

cat > "${OUT_DIR}/candidates.txt" <<'EOF'
/data/work/datasets/openai-arxiv
/data/work/datasets/simplewiki-openai
/data/work/datasets/crisp/simplewiki-openai
/data/work/datasets/wikipedia-cohere-1m
EOF

"${REMOTE_TOOLS_DIR}/run.sh" \
  --workdir "${REMOTE_PROBE_DIR}" \
  -- python3 probe_openai_arxiv_format_remote.py files \
  > "${OUT_DIR}/files.txt"

"${REMOTE_TOOLS_DIR}/run.sh" \
  --workdir "${REMOTE_PROBE_DIR}" \
  -- python3 probe_openai_arxiv_format_remote.py headers \
  > "${OUT_DIR}/headers.txt"

echo "probe=ok"
echo "out_dir=${OUT_DIR}"
