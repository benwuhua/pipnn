#!/usr/bin/env bash
set -euo pipefail

: "${SIFT1M_DIR:=/data/work/knowhere-rs-src/data/sift}"
: "${OUT_DIR:=results/tuning}"
: "${MAX_BASE:=100000}"
: "${MAX_QUERY:=100}"

mkdir -p "${OUT_DIR}"

BASE_FVECS="${SIFT1M_DIR}/base.fvecs"
QUERY_FVECS="${SIFT1M_DIR}/query.fvecs"

if [[ ! -f "${BASE_FVECS}" || ! -f "${QUERY_FVECS}" ]]; then
  echo "missing dataset files under ${SIFT1M_DIR}" >&2
  exit 1
fi

run_pipnn() {
  local name="$1"
  local cmax="$2"
  local fanout="$3"
  local lfrac="$4"
  local mleaders="$5"
  local leafk="$6"
  local maxdeg="$7"
  local hbits="$8"
  local beam="$9"

  ./build/pipnn \
    --mode pipnn --dataset sift1m --metric l2 \
    --base "${BASE_FVECS}" --query "${QUERY_FVECS}" \
    --max-base "${MAX_BASE}" --max-query "${MAX_QUERY}" \
    --rbc-cmax "${cmax}" --rbc-fanout "${fanout}" \
    --leader-frac "${lfrac}" --max-leaders "${mleaders}" \
    --leaf-k "${leafk}" --max-degree "${maxdeg}" \
    --hash-bits "${hbits}" --beam "${beam}" \
    --output "${OUT_DIR}/${name}.json" >/dev/null
}

run_hnsw() {
  ./build/pipnn \
    --mode hnsw --dataset sift1m --metric l2 \
    --base "${BASE_FVECS}" --query "${QUERY_FVECS}" \
    --max-base "${MAX_BASE}" --max-query "${MAX_QUERY}" \
    --output "${OUT_DIR}/hnsw_baseline.json" >/dev/null
}

echo "running hnsw baseline..."
run_hnsw

echo "running pipnn configs..."
run_pipnn "pipnn_fast_a" 64 1 0.002 32 8 24 8 96
run_pipnn "pipnn_fast_b" 96 1 0.003 48 12 32 10 128
run_pipnn "pipnn_bal_a" 128 1 0.003 64 16 40 10 128
run_pipnn "pipnn_bal_b" 128 2 0.008 96 20 48 12 160
run_pipnn "pipnn_quality_a" 128 2 0.02 1000 12 32 12 128
run_pipnn "pipnn_quality_b" 256 3 0.004 128 32 64 10 256

python3 - "${OUT_DIR}" <<'PY'
import json
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
rows = []
for p in sorted(out.glob("*.json")):
    d = json.loads(p.read_text())
    rows.append((p.stem, d["mode"], d["build_sec"], d["recall_at_10"], d["qps"], d["edges"]))

tsv = out / "summary.tsv"
with tsv.open("w") as f:
    f.write("name\tmode\tbuild_sec\trecall_at_10\tqps\tedges\n")
    for r in rows:
        f.write(f"{r[0]}\t{r[1]}\t{r[2]:.6f}\t{r[3]:.6f}\t{r[4]:.6f}\t{r[5]}\n")

print(tsv.read_text())
PY

