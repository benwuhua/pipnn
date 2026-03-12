#!/usr/bin/env bash
set -euo pipefail

: "${SIMPLEWIKI_DIR:=/data/work/datasets/crisp/simplewiki-openai}"
: "${OUT_DIR:=results/simplewiki_openai_20k_100}"
: "${MAX_BASE:=20000}"
: "${MAX_QUERY:=100}"
: "${RBC_CMAX:=128}"
: "${RBC_FANOUT:=2}"
: "${LEADER_FRAC:=0.02}"
: "${MAX_LEADERS:=1000}"
: "${REPLICAS:=1}"
: "${LEAF_K:=12}"
: "${LEAF_SCAN_CAP:=0}"
: "${MAX_DEGREE:=32}"
: "${HASH_BITS:=12}"
: "${BEAM:=128}"
: "${BIDIRECTED:=1}"

mkdir -p "${OUT_DIR}"

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

BASE_FVECS="${SIMPLEWIKI_DIR}/base.fvecs"
QUERY_FVECS="${SIMPLEWIKI_DIR}/query.fvecs"

PIPNN_PROFILE=1 ./build/pipnn \
  --mode pipnn \
  --dataset sift1m \
  --metric l2 \
  --base "${BASE_FVECS}" \
  --query "${QUERY_FVECS}" \
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
  > "${OUT_DIR}/pipnn.stdout"

./build/pipnn \
  --mode hnsw \
  --dataset sift1m \
  --metric l2 \
  --base "${BASE_FVECS}" \
  --query "${QUERY_FVECS}" \
  --max-base "${MAX_BASE}" \
  --max-query "${MAX_QUERY}" \
  --output "${OUT_DIR}/hnsw_metrics.json" \
  > "${OUT_DIR}/hnsw.stdout"
