#!/usr/bin/env bash
set -euo pipefail

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found" >&2
  exit 1
fi

if ! command -v c++ >/dev/null 2>&1; then
  echo "c++ compiler not found" >&2
  exit 1
fi

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure || true

echo "init.sh complete"
