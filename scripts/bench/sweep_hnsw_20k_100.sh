#!/usr/bin/env bash
set -euo pipefail

: "${SIMPLEWIKI_DIR:=/data/work/datasets/crisp/simplewiki-openai}"
: "${OUT_DIR:=results/high_dim_validation/hnsw_sweep_20k_100}"
: "${MAX_BASE:=20000}"
: "${MAX_QUERY:=100}"
: "${M_LIST:=16 24 32}"
: "${EFC_LIST:=200 400}"
: "${EFS_LIST:=0 64 96 128 160}"

mkdir -p "${OUT_DIR}"

BASE_FVECS="${SIMPLEWIKI_DIR}/base.fvecs"
QUERY_FVECS="${SIMPLEWIKI_DIR}/query.fvecs"

if [[ ! -f "${BASE_FVECS}" || ! -f "${QUERY_FVECS}" ]]; then
  echo "missing dataset files under ${SIMPLEWIKI_DIR}" >&2
  exit 1
fi

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

for m in ${M_LIST}; do
  for efc in ${EFC_LIST}; do
    for efs in ${EFS_LIST}; do
      if [[ "${efs}" == "0" ]]; then
        efs_tag="auto"
      else
        efs_tag="${efs}"
      fi
      name="hnsw_m${m}_efc${efc}_efs${efs_tag}"
      echo "running ${name} ..."
      ./build/pipnn \
        --mode hnsw --dataset sift1m --metric l2 \
        --base "${BASE_FVECS}" --query "${QUERY_FVECS}" \
        --max-base "${MAX_BASE}" --max-query "${MAX_QUERY}" \
        --hnsw-m "${m}" --hnsw-ef-construction "${efc}" --hnsw-ef-search "${efs}" \
        --output "${OUT_DIR}/${name}.json" \
        > "${OUT_DIR}/${name}.stdout"
    done
  done
done

python3 - "${OUT_DIR}" <<'PY'
import json
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
rows = []
for p in sorted(out.glob("hnsw_*.json")):
    d = json.loads(p.read_text())
    rows.append((p.stem, d["build_sec"], d["recall_at_10"], d["qps"], d["edges"]))

rows.sort(key=lambda x: (-x[2], -x[3], x[1]))
summary = out / "summary.tsv"
with summary.open("w") as f:
    f.write("name\tbuild_sec\trecall_at_10\tqps\tedges\n")
    for row in rows:
        f.write(f"{row[0]}\t{row[1]:.6f}\t{row[2]:.6f}\t{row[3]:.6f}\t{row[4]}\n")

print(summary.read_text())
PY
