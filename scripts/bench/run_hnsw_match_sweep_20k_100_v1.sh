#!/usr/bin/env bash
set -euo pipefail

# Targeted HNSW sweep around lower recall to build a recall-matched fairness curve at 20k/100.
M_LIST="8 12 16"
EFC_LIST="100"
EFS_LIST="8 16 24"
OUT_DIR="results/high_dim_validation/hnsw_sweep_20k_100_match_v1"
MAX_BASE="20000"
MAX_QUERY="100"

M_LIST="${M_LIST}" \
EFC_LIST="${EFC_LIST}" \
EFS_LIST="${EFS_LIST}" \
OUT_DIR="${OUT_DIR}" \
MAX_BASE="${MAX_BASE}" \
MAX_QUERY="${MAX_QUERY}" \
scripts/bench/sweep_hnsw_20k_100.sh
