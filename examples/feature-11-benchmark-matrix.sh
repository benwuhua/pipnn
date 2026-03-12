#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cat <<'EOF'
Feature 11 example: inspect the fixed SIFT1M benchmark matrix artifacts.

This example does not launch remote jobs. It reads the checked-in local artifacts:
- results/pipnn_vs_hnsw_sift1m.md
- results/bench_500k_100_subset/pipnn_metrics.json
- results/bench_500k_100_subset/hnsw_metrics.json
EOF

echo
echo "== 500k subset-truth PiPNN =="
cat "${ROOT}/results/bench_500k_100_subset/pipnn_metrics.json"
echo
echo "== 500k subset-truth HNSW =="
cat "${ROOT}/results/bench_500k_100_subset/hnsw_metrics.json"
echo
echo "== Report excerpt =="
awk '/^### 500k base \/ 100 query/{flag=1} /^### 30k base \/ 200 query/{flag=0} flag {print}' "${ROOT}/results/pipnn_vs_hnsw_sift1m.md"
