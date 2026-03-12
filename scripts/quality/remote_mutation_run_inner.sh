#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MODE="full"
WORKERS="${PIPNN_MULL_WORKERS:-1}"
REPORT_ROOT="${PIPNN_MUTATION_REPORT_ROOT:-${REPO_ROOT}/results/st/mutation}"
EXECUTABLE_DIR="${PIPNN_MUTATION_EXECUTABLE_DIR:-}"
TOOLCHAIN_FILE="${PIPNN_MUTATION_TOOLCHAIN_FILE:-}"

SMOKE_TARGETS=(test_hashprune)
FULL_TARGETS=(test_cli_app test_hashprune test_leaf_knn test_rbc test_pipnn_integration)

fail() {
  echo "$1" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage: bash scripts/quality/remote_mutation_run_inner.sh [--mode smoke|full] [--workers N]

Runs the remote mutation pipeline against the approved targeted executable set and
stores raw Elements JSON reports under results/st/mutation/.

Env overrides:
  PIPNN_MUTATION_REPORT_ROOT     Alternate report root
  PIPNN_MUTATION_EXECUTABLE_DIR  Skip build-mull and read executables from this dir
  PIPNN_MUTATION_TOOLCHAIN_FILE  Reuse a previously generated toolchain path file
  PIPNN_MULL_WORKERS             Default worker count
EOF
}

while (($#)); do
  case "$1" in
    --mode)
      MODE="$2"
      shift 2
      ;;
    --workers)
      WORKERS="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

case "${MODE}" in
  smoke|full) ;;
  *)
    fail "unsupported mode: ${MODE}"
    ;;
esac

if [[ -z "${TOOLCHAIN_FILE}" ]]; then
  TOOLCHAIN_FILE="${REPORT_ROOT}/toolchain_paths.env"
  mkdir -p "${REPORT_ROOT}"
  bash "${REPO_ROOT}/scripts/quality/ensure_remote_mull_toolchain.sh" --print-paths >"${TOOLCHAIN_FILE}"
fi

declare CLANG=""
declare CLANGXX=""
declare MULL_ROOT=""
declare MULL_RUNNER=""
declare MULL_LLVM_MAJOR=""
declare LLVM_VERSION=""
declare MULL_VERSION=""
declare BUILD_CLANG=""
declare BUILD_CLANGXX=""
declare BUILD_LLVM_CONFIG=""

while IFS='=' read -r key value; do
  case "${key}" in
    clang) CLANG="${value}" ;;
    clangxx) CLANGXX="${value}" ;;
    mull_root) MULL_ROOT="${value}" ;;
    mull_runner) MULL_RUNNER="${value}" ;;
    mull_llvm_major) MULL_LLVM_MAJOR="${value}" ;;
    llvm_version) LLVM_VERSION="${value}" ;;
    mull_version) MULL_VERSION="${value}" ;;
  esac
done <"${TOOLCHAIN_FILE}"

[[ -x "${CLANG}" ]] || fail "missing clang from toolchain file: ${CLANG}"
[[ -x "${CLANGXX}" ]] || fail "missing clang++ from toolchain file: ${CLANGXX}"
[[ -x "${MULL_RUNNER}" ]] || fail "missing mull-runner from toolchain file: ${MULL_RUNNER}"
[[ -n "${MULL_ROOT}" ]] || fail "missing mull_root in toolchain file"
[[ -n "${MULL_LLVM_MAJOR}" ]] || fail "missing mull_llvm_major in toolchain file"

PLUGIN_PATH="${MULL_ROOT}/lib/mull-ir-frontend-${MULL_LLVM_MAJOR}"
[[ -f "${PLUGIN_PATH}" ]] || fail "missing Mull pass plugin: ${PLUGIN_PATH}"

BUILD_COMPILER_FILE="${REPORT_ROOT}/build_compiler_paths.env"
bash "${REPO_ROOT}/scripts/quality/ensure_remote_mull_build_compiler.sh" --print-paths >"${BUILD_COMPILER_FILE}"
while IFS='=' read -r key value; do
  case "${key}" in
    build_clang) BUILD_CLANG="${value}" ;;
    build_clangxx) BUILD_CLANGXX="${value}" ;;
    build_llvm_config) BUILD_LLVM_CONFIG="${value}" ;;
  esac
done <"${BUILD_COMPILER_FILE}"

[[ -x "${BUILD_CLANG}" ]] || fail "missing build clang path: ${BUILD_CLANG}"
[[ -x "${BUILD_CLANGXX}" ]] || fail "missing build clang++ path: ${BUILD_CLANGXX}"
[[ -x "${BUILD_LLVM_CONFIG}" ]] || fail "missing build llvm-config path: ${BUILD_LLVM_CONFIG}"

if [[ "${MODE}" == "smoke" ]]; then
  TARGETS=("${SMOKE_TARGETS[@]}")
else
  TARGETS=("${FULL_TARGETS[@]}")
fi

REPORT_DIR="${REPORT_ROOT}/${MODE}"
rm -rf "${REPORT_DIR}"
mkdir -p "${REPORT_DIR}"

if [[ -z "${EXECUTABLE_DIR}" ]]; then
  BUILD_DIR="${REPO_ROOT}/build-mull"
  rm -rf "${BUILD_DIR}"
  export MULL_CONFIG="${REPO_ROOT}/scripts/quality/mull.yml"
  cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_CXX_COMPILER="${BUILD_CLANGXX}" \
    -DCMAKE_CXX_FLAGS="-O0 -fpass-plugin=${PLUGIN_PATH} -g -grecord-command-line" \
    -DPIPNN_ENABLE_OPENMP=OFF
  cmake --build "${BUILD_DIR}" -j --target "${TARGETS[@]}"
  EXECUTABLE_DIR="${BUILD_DIR}/tests"
fi

[[ -d "${EXECUTABLE_DIR}" ]] || fail "missing executable directory: ${EXECUTABLE_DIR}"

MANIFEST="${REPORT_DIR}/run_manifest.txt"
{
  echo "mode=${MODE}"
  echo "report_dir=${REPORT_DIR}"
  echo "executable_dir=${EXECUTABLE_DIR}"
  echo "clang=${CLANG}"
  echo "clangxx=${CLANGXX}"
  echo "build_clang=${BUILD_CLANG}"
  echo "build_clangxx=${BUILD_CLANGXX}"
  echo "build_llvm_config=${BUILD_LLVM_CONFIG}"
  echo "mull_runner=${MULL_RUNNER}"
  echo "llvm_version=${LLVM_VERSION}"
  echo "mull_version=${MULL_VERSION}"
  echo "workers=${WORKERS}"
} >"${MANIFEST}"

for target in "${TARGETS[@]}"; do
  executable_path="${EXECUTABLE_DIR}/${target}"
  [[ -x "${executable_path}" ]] || fail "missing mutation target executable: ${executable_path}"

  report_name="${target}"
  log_path="${REPORT_DIR}/${target}.log"
  env -u LD_LIBRARY_PATH "${MULL_RUNNER}" \
    --allow-surviving \
    --reporters Elements \
    --report-dir "${REPORT_DIR}" \
    --report-name "${report_name}" \
    --workers "${WORKERS}" \
    "${executable_path}" >"${log_path}" 2>&1 || {
      cat "${log_path}" >&2
      fail "mull-runner failed for ${target}"
    }

  [[ -f "${REPORT_DIR}/${target}.json" ]] || fail "missing raw report: ${REPORT_DIR}/${target}.json"
  {
    echo "target=${target}"
    echo "report=${REPORT_DIR}/${target}.json"
    echo "log=${log_path}"
  } >>"${MANIFEST}"
done

echo "mutation=ok"
echo "mode=${MODE}"
echo "report_dir=${REPORT_DIR}"
echo "manifest=${MANIFEST}"
