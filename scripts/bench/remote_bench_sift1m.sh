#!/usr/bin/env bash
set -euo pipefail

: "${SIFT1M_DIR:=/data/datasets/sift1m}"
: "${OUT_DIR:=results}"
: "${MAX_BASE:=0}"
: "${MAX_QUERY:=0}"
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

BASE_FVECS="${SIFT1M_DIR}/base.fvecs"
QUERY_FVECS="${SIFT1M_DIR}/query.fvecs"
TRUTH_IVECS="${SIFT1M_DIR}/groundtruth.ivecs"

./build/pipnn --mode pipnn --dataset sift1m --metric l2 --base "${BASE_FVECS}" --query "${QUERY_FVECS}" --truth "${TRUTH_IVECS}" --max-base "${MAX_BASE}" --max-query "${MAX_QUERY}" --rbc-cmax "${RBC_CMAX}" --rbc-fanout "${RBC_FANOUT}" --leader-frac "${LEADER_FRAC}" --max-leaders "${MAX_LEADERS}" --replicas "${REPLICAS}" --leaf-k "${LEAF_K}" --leaf-scan-cap "${LEAF_SCAN_CAP}" --max-degree "${MAX_DEGREE}" --hash-bits "${HASH_BITS}" --beam "${BEAM}" --bidirected "${BIDIRECTED}" --output "${OUT_DIR}/pipnn_metrics.json"
./build/pipnn --mode hnsw --dataset sift1m --metric l2 --base "${BASE_FVECS}" --query "${QUERY_FVECS}" --truth "${TRUTH_IVECS}" --max-base "${MAX_BASE}" --max-query "${MAX_QUERY}" --output "${OUT_DIR}/hnsw_metrics.json"
