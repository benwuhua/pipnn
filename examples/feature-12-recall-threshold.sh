#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

check_recall() {
  local file="$1"
  local label="$2"
  local recall
  recall="$(awk -F': ' '/"recall_at_10"/ {gsub(/,/, "", $2); print $2}' "${file}")"
  echo "${label}: recall_at_10=${recall}"
}

echo "Feature 12 example: verify the fixed beam=384 quality config"
echo

check_recall "${ROOT}/results/feature12_100k_beam384.json" "100k/100"
check_recall "${ROOT}/results/feature12_200k_beam384.json" "200k/100"
check_recall "${ROOT}/results/feature12_500k_beam384.json" "500k/100"
