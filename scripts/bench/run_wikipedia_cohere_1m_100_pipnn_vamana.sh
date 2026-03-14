#!/usr/bin/env bash
set -euo pipefail

: "${WIKIPEDIA_COHERE_DIR:=/data/work/datasets/wikipedia-cohere-1m}"
: "${OUT_DIR:=results/wikipedia_cohere_1m_100}"
: "${METRIC:=ip}"
: "${MAX_BASE:=0}"
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
: "${BEAM:=384}"
: "${BIDIRECTED:=1}"

mkdir -p "${OUT_DIR}"

BASE_FBIN="${WIKIPEDIA_COHERE_DIR}/base.fbin"
QUERY_FBIN="${WIKIPEDIA_COHERE_DIR}/query.fbin"
TRUTH_IBIN="${WIKIPEDIA_COHERE_DIR}/gt.ibin"

if [[ ! -f "${BASE_FBIN}" || ! -f "${QUERY_FBIN}" || ! -f "${TRUTH_IBIN}" ]]; then
  echo "missing dataset files under ${WIKIPEDIA_COHERE_DIR}" >&2
  exit 1
fi

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

PIPNN_PROFILE=1 ./build/pipnn \
  --mode pipnn_vamana \
  --dataset file \
  --metric "${METRIC}" \
  --base "${BASE_FBIN}" \
  --query "${QUERY_FBIN}" \
  --truth "${TRUTH_IBIN}" \
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
  --output "${OUT_DIR}/pipnn_vamana_metrics.json" \
  > "${OUT_DIR}/pipnn_vamana.stdout"
