#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

if [[ ! -x build/pipnn ]]; then
  cmake -S . -B build
  cmake --build build -j
fi

python3 scripts/check_real_tests.py feature-list.json --feature 1

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

./build/pipnn --help | grep -F -- "Usage: pipnn" >/dev/null

./build/pipnn --mode hnsw --dataset synthetic --metric l2 --output "${TMP_DIR}/hnsw.json"
grep -F '"mode": "hnsw"' "${TMP_DIR}/hnsw.json" >/dev/null

set +e
./build/pipnn --mode pipnn --dataset nope --metric l2 --output "${TMP_DIR}/bad.json" \
  >"${TMP_DIR}/bad.stdout" 2>"${TMP_DIR}/bad.stderr"
status=$?
set -e
test "${status}" -ne 0
grep -F "unsupported dataset: nope" "${TMP_DIR}/bad.stderr" >/dev/null

set +e
./build/pipnn --mode pipnn --dataset synthetic --metric l2 --output "${TMP_DIR}/bad-beam.json" --beam nope \
  >"${TMP_DIR}/beam.stdout" 2>"${TMP_DIR}/beam.stderr"
status=$?
set -e
test "${status}" -ne 0
grep -F "invalid value for --beam: nope" "${TMP_DIR}/beam.stderr" >/dev/null

set +e
./build/pipnn --mode pipnn --dataset sift1m --metric l2 \
  --base "${TMP_DIR}/missing-base.fvecs" --query "${TMP_DIR}/missing-query.fvecs" \
  --output "${TMP_DIR}/missing.json" \
  >"${TMP_DIR}/missing.stdout" 2>"${TMP_DIR}/missing.stderr"
status=$?
set -e
test "${status}" -ne 0
grep -F "cannot open" "${TMP_DIR}/missing.stderr" >/dev/null

WORK_DIR="${TMP_DIR}/basename"
mkdir -p "${WORK_DIR}"
(
  cd "${WORK_DIR}"
  "${REPO_ROOT}/build/pipnn" --mode pipnn --dataset synthetic --metric l2 --output metrics.json
)
test -f "${WORK_DIR}/metrics.json"
