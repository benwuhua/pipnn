#!/usr/bin/env bash
set -euo pipefail

src_env="${GENERIC_X86_REMOTE_ENV:-$HOME/.config/generic-x86-remote/remote.env}"
if [[ ! -f "$src_env" ]]; then
  echo "missing generic remote env: $src_env" >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "$src_env"
set +a

if [[ -z "${REMOTE_HOST:-}" || -z "${REMOTE_USER:-}" ]]; then
  echo "REMOTE_HOST/REMOTE_USER not configured" >&2
  exit 1
fi

ssh_args=(-o StrictHostKeyChecking=accept-new -p "${REMOTE_PORT:-22}")
if [[ -n "${SSH_IDENTITY_FILE:-}" ]]; then
  ssh_args+=(-i "${SSH_IDENTITY_FILE}" -o IdentitiesOnly=yes)
fi

target="${REMOTE_USER}@${REMOTE_HOST}"
probe_dir="${REMOTE_WORK_ROOT:-/data/work}/diskann_cpp_probe"

ssh "${ssh_args[@]}" "$target" "set -euo pipefail
cd '${REMOTE_WORK_ROOT:-/data/work}'
rm -rf '${probe_dir}'
git clone --depth 1 --branch cpp_main --recurse-submodules https://github.com/microsoft/DiskANN.git '${probe_dir}'
cd '${probe_dir}'
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .."
