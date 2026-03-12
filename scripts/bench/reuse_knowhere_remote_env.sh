#!/usr/bin/env bash
set -euo pipefail

src_env="${KNOWHERE_REMOTE_ENV:-$HOME/.config/knowhere-x86-remote/remote.env}"
dst_env="${GENERIC_X86_REMOTE_ENV:-$HOME/.config/generic-x86-remote/remote.env}"

if [[ ! -f "$src_env" ]]; then
  echo "missing knowhere remote env: $src_env" >&2
  exit 1
fi

mkdir -p "$(dirname "$dst_env")"

set -a
# shellcheck disable=SC1090
source "$src_env"
set +a

cat > "$dst_env" <<CFG
REMOTE_HOST=${REMOTE_HOST:-}
REMOTE_USER=${REMOTE_USER:-}
REMOTE_PORT=${REMOTE_PORT:-22}
REMOTE_WORK_ROOT=${REMOTE_WORK_ROOT:-/data/work}
REMOTE_REPO_DIR=${REMOTE_REPO_DIR:-/data/work/pipnn}
REMOTE_BUILD_DIR=${REMOTE_BUILD_DIR:-/data/work/pipnn/build}
REMOTE_LOG_DIR=${REMOTE_LOG_DIR:-/data/work/logs}
REMOTE_CCACHE_DIR=${REMOTE_CCACHE_DIR:-/data/work/ccache}
SSH_IDENTITY_FILE=${SSH_IDENTITY_FILE:-}
CFG

chmod 600 "$dst_env"
echo "generic-x86 remote env updated: $dst_env"
