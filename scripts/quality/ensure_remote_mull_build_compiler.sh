#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LLVM_VERSION="${PIPNN_LLVM_VERSION:-17.0.6}"
LLVM_MAJOR="${LLVM_VERSION%%.*}"
PACKAGE_VERSION="${PIPNN_LLVM_APT_VERSION:-1:17.0.6-9ubuntu1}"
PACKAGE_ARCH="${PIPNN_LLVM_APT_ARCH:-amd64}"

TOOLCHAIN_ROOT="${PIPNN_TOOLCHAIN_ROOT:-${REPO_ROOT}/.tools}"
DOWNLOAD_ROOT="${TOOLCHAIN_ROOT}/downloads"
BUILD_ROOT="${TOOLCHAIN_ROOT}/llvm-build/${LLVM_VERSION}"
BUILD_CURRENT="${TOOLCHAIN_ROOT}/llvm-build/current"

PACKAGE_NAMES=(
  clang-17
  llvm-17
  llvm-17-dev
  llvm-17-tools
  llvm-17-linker-tools
  libclang-common-17-dev
  libclang-cpp17t64
  libclang1-17t64
)

PRINT_PATHS=0

fail() {
  echo "$1" >&2
  exit 1
}

log() {
  echo "[ensure-remote-mull-build-compiler] $1" >&2
}

usage() {
  cat <<'EOF'
Usage: bash scripts/quality/ensure_remote_mull_build_compiler.sh [--print-paths]

Installs a repo-local Ubuntu-packaged clang/llvm toolchain compatible with Mull's
LLVM 17 shared-library ABI for build-mull usage.
EOF
}

download_apt_package() {
  local package_name="$1"
  local package_version="$2"
  local package_arch="$3"
  local encoded_version="${package_version//:/%3a}"
  local pattern="${package_name}_${encoded_version}_${package_arch}.deb"
  local existing
  existing="$(find "${DOWNLOAD_ROOT}" -maxdepth 1 -type f -name "${pattern}" | head -n 1)"
  if [[ -n "${existing}" ]]; then
    printf '%s\n' "${existing}"
    return
  fi
  command -v apt >/dev/null 2>&1 || fail "apt is required to download ${package_name}"
  log "downloading ${package_name}=${package_version}"
  (
    cd "${DOWNLOAD_ROOT}"
    apt download "${package_name}=${package_version}" >/dev/null
  )
  existing="$(find "${DOWNLOAD_ROOT}" -maxdepth 1 -type f -name "${pattern}" | head -n 1)"
  [[ -n "${existing}" ]] || fail "failed to download ${package_name}=${package_version}"
  printf '%s\n' "${existing}"
}

extract_deb_package() {
  local package="$1"
  local dest_dir="$2"
  local stage
  stage="$(mktemp -d)"
  if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb -x "${package}" "${stage}"
  else
    command -v ar >/dev/null 2>&1 || fail "dpkg-deb or ar is required to unpack ${package}"
    local ar_stage="${stage}/ar"
    mkdir -p "${ar_stage}"
    (cd "${ar_stage}" && ar x "${package}")
    local data_archive
    data_archive="$(find "${ar_stage}" -maxdepth 1 -type f \( -name 'data.tar.xz' -o -name 'data.tar.zst' -o -name 'data.tar.gz' \) | head -n 1)"
    [[ -n "${data_archive}" ]] || fail "unable to locate data archive inside ${package}"
    tar -xf "${data_archive}" -C "${stage}"
  fi
  mkdir -p "${dest_dir}"
  cp -R "${stage}/." "${dest_dir}/"
  rm -rf "${stage}"
}

make_wrapper() {
  local path="$1"
  local target="$2"
  cat >"${path}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
ROOT="\$(cd "\$(dirname "\$0")/.." && pwd)"
export LD_LIBRARY_PATH="\${ROOT}/usr/lib/x86_64-linux-gnu:\${ROOT}/usr/lib/llvm-${LLVM_MAJOR}/lib\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}"
exec "\${ROOT}/${target}" "\$@"
EOF
  chmod +x "${path}"
}

require_layout() {
  [[ -x "${BUILD_ROOT}/usr/bin/clang-${LLVM_MAJOR}" ]] || fail \
    "missing clang package binary: ${BUILD_ROOT}/usr/bin/clang-${LLVM_MAJOR}"
  [[ -x "${BUILD_ROOT}/usr/bin/clang++-${LLVM_MAJOR}" ]] || fail \
    "missing clang++ package binary: ${BUILD_ROOT}/usr/bin/clang++-${LLVM_MAJOR}"
  [[ -x "${BUILD_ROOT}/usr/lib/llvm-${LLVM_MAJOR}/bin/llvm-config" ]] || fail \
    "missing llvm-config package binary: ${BUILD_ROOT}/usr/lib/llvm-${LLVM_MAJOR}/bin/llvm-config"
}

while (($#)); do
  case "$1" in
    --print-paths)
      PRINT_PATHS=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
  shift
done

mkdir -p "${DOWNLOAD_ROOT}"

if [[ ! -x "${BUILD_ROOT}/bin/clang" || ! -x "${BUILD_ROOT}/bin/clang++" || ! -x "${BUILD_ROOT}/bin/llvm-config" ]]; then
  rm -rf "${BUILD_ROOT}"
  mkdir -p "${BUILD_ROOT}/bin"
  for package_name in "${PACKAGE_NAMES[@]}"; do
    package_path="$(download_apt_package "${package_name}" "${PACKAGE_VERSION}" "${PACKAGE_ARCH}")"
    extract_deb_package "${package_path}" "${BUILD_ROOT}"
  done
  require_layout
  make_wrapper "${BUILD_ROOT}/bin/clang" "usr/bin/clang-${LLVM_MAJOR}"
  make_wrapper "${BUILD_ROOT}/bin/clang++" "usr/bin/clang++-${LLVM_MAJOR}"
  make_wrapper "${BUILD_ROOT}/bin/llvm-config" "usr/lib/llvm-${LLVM_MAJOR}/bin/llvm-config"
fi

require_layout
ln -sfn "${BUILD_ROOT}" "${BUILD_CURRENT}"

clang_version="$("${BUILD_CURRENT}/bin/clang" --version | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)"
[[ "${clang_version}" == "${LLVM_VERSION}" ]] || fail \
  "llvm build compiler version mismatch: expected ${LLVM_VERSION}, got ${clang_version:-unknown}"

if ((PRINT_PATHS)); then
  echo "build_root=${BUILD_CURRENT}"
  echo "build_clang=${BUILD_CURRENT}/bin/clang"
  echo "build_clangxx=${BUILD_CURRENT}/bin/clang++"
  echo "build_llvm_config=${BUILD_CURRENT}/bin/llvm-config"
  echo "build_llvm_version=${LLVM_VERSION}"
fi
