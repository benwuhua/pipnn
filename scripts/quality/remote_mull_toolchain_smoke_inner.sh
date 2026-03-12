#!/usr/bin/env bash

set -euo pipefail

mkdir -p results/st
bash scripts/quality/ensure_remote_mull_toolchain.sh --print-paths | tee results/st/mull_toolchain_paths.txt
