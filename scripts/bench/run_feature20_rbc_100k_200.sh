#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${SIFT1M_DIR:=/data/work/knowhere-rs-src/data/sift}"
: "${OUT_DIR:=${ROOT}/results/feature20_100k_200}"
: "${MAX_BASE:=100000}"
: "${MAX_QUERY:=200}"
: "${RBC_CMAX:=128}"
: "${RBC_FANOUT:=1}"
: "${LEADER_FRAC:=0.02}"
: "${MAX_LEADERS:=128}"
: "${REPLICAS:=2}"
: "${LEAF_K:=12}"
: "${LEAF_SCAN_CAP:=0}"
: "${MAX_DEGREE:=32}"
: "${HASH_BITS:=12}"
: "${BEAM:=384}"
: "${BIDIRECTED:=1}"

mkdir -p "${OUT_DIR}"
cmake -S "${ROOT}" -B "${ROOT}/build"
cmake --build "${ROOT}/build" -j
ctest --test-dir "${ROOT}/build" --output-on-failure
PIPNN_PROFILE=1 "${ROOT}/build/pipnn" \
  --mode pipnn \
  --dataset sift1m \
  --metric l2 \
  --base "${SIFT1M_DIR}/base.fvecs" \
  --query "${SIFT1M_DIR}/query.fvecs" \
  --max-base "${MAX_BASE}" \
  --max-query "${MAX_QUERY}" \
  --rbc-cmax "${RBC_CMAX}" \
  --rbc-fanout "${RBC_FANOUT}" \
  --leader-frac "${LEADER_FRAC}" \
  --max-leaders "${MAX_LEADERS}" \
  --replicas "${REPLICAS}" \
  --leaf-k "${LEAF_K}" \
  --leaf-scan-cap "${LEAF_SCAN_CAP}" \
  --max-degree "${MAX_DEGREE}" \
  --hash-bits "${HASH_BITS}" \
  --beam "${BEAM}" \
  --bidirected "${BIDIRECTED}" \
  --output "${OUT_DIR}/pipnn_metrics.json" \
  > "${OUT_DIR}/pipnn.stdout" 2>&1
