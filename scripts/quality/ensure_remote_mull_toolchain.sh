#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LLVM_VERSION="${PIPNN_LLVM_VERSION:-17.0.6}"
LLVM_MAJOR="${LLVM_VERSION%%.*}"
MULL_VERSION="${PIPNN_MULL_VERSION:-0.29.0}"
MULL_LLVM_MAJOR="${PIPNN_MULL_LLVM_MAJOR:-17}"
CLANG_CPP_PACKAGE_NAME="${PIPNN_CLANG_CPP_PACKAGE_NAME:-libclang-cpp17t64}"
CLANG_CPP_PACKAGE_VERSION="${PIPNN_CLANG_CPP_PACKAGE_VERSION:-1:17.0.6-9ubuntu1}"
CLANG_CPP_ARCH="${PIPNN_CLANG_CPP_ARCH:-amd64}"

TOOLCHAIN_ROOT="${PIPNN_TOOLCHAIN_ROOT:-${REPO_ROOT}/.tools}"
DOWNLOAD_ROOT="${TOOLCHAIN_ROOT}/downloads"
LLVM_ROOT="${TOOLCHAIN_ROOT}/llvm/${LLVM_VERSION}"
LLVM_CURRENT="${TOOLCHAIN_ROOT}/llvm/current"
MULL_ROOT="${TOOLCHAIN_ROOT}/mull/${MULL_VERSION}"
MULL_CURRENT="${TOOLCHAIN_ROOT}/mull/current"
MULL_COMPAT_REL="compat/lib/x86_64-linux-gnu"

LLVM_ARCHIVE_NAME="clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
LLVM_URL="${PIPNN_LLVM_DOWNLOAD_URL:-https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/${LLVM_ARCHIVE_NAME}}"
MULL_PACKAGE_NAME="Mull-${MULL_LLVM_MAJOR}-${MULL_VERSION}-LLVM-${LLVM_VERSION}-ubuntu-amd64-24.04.deb"
MULL_URL="${PIPNN_MULL_DOWNLOAD_URL:-https://github.com/mull-project/mull/releases/download/${MULL_VERSION}/${MULL_PACKAGE_NAME}}"

LLVM_SOURCE_DIR="${PIPNN_LLVM_SOURCE_DIR:-}"
MULL_SOURCE_DIR="${PIPNN_MULL_SOURCE_DIR:-}"

PRINT_PATHS=0

usage() {
  cat <<'EOF'
Usage: bash scripts/quality/ensure_remote_mull_toolchain.sh [--print-paths]

Installs a repo-local LLVM + Mull toolchain into .tools/ and prints stable paths
when --print-paths is provided.

Override knobs:
  PIPNN_TOOLCHAIN_ROOT      Alternate .tools root
  PIPNN_LLVM_SOURCE_DIR     Copy LLVM tree from a prepared directory instead of downloading
  PIPNN_MULL_SOURCE_DIR     Copy Mull tree from a prepared directory instead of downloading
  PIPNN_LLVM_DOWNLOAD_URL   Override LLVM archive URL
  PIPNN_MULL_DOWNLOAD_URL   Override Mull package URL
EOF
}

fail() {
  echo "$1" >&2
  exit 1
}

log() {
  echo "[ensure-remote-mull] $1" >&2
}

extract_semver() {
  grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1
}

download_file() {
  local url="$1"
  local output="$2"
  mkdir -p "$(dirname "${output}")"
  if [[ -f "${output}" ]]; then
    return
  fi
  if ! command -v curl >/dev/null 2>&1; then
    fail "curl is required to download toolchain assets"
  fi
  log "downloading $(basename "${output}")"
  curl -fL --retry 3 --retry-delay 2 -o "${output}" "${url}"
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

copy_tree() {
  local source_dir="$1"
  local dest_dir="$2"
  [[ -d "${source_dir}" ]] || fail "missing source directory: ${source_dir}"
  cmake -E copy_directory "${source_dir}" "${dest_dir}"
}

extract_llvm_archive() {
  local archive="$1"
  local dest_dir="$2"
  local stage
  stage="$(mktemp -d)"
  tar -xf "${archive}" -C "${stage}"
  local unpacked
  unpacked="$(find "${stage}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  [[ -n "${unpacked}" ]] || fail "unable to locate extracted llvm directory in ${archive}"
  cmake -E copy_directory "${unpacked}" "${dest_dir}"
  rm -rf "${stage}"
}

extract_mull_package() {
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
  [[ -d "${stage}/usr" ]] || fail "unexpected mull package layout in ${package}"
  mkdir -p "${dest_dir}"
  cp -R "${stage}/usr/." "${dest_dir}/"
  rm -rf "${stage}"
}

make_wrapper() {
  local path="$1"
  local target_name="$2"
  cat >"${path}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
ROOT="\$(cd "\$(dirname "\$0")/.." && pwd)"
export PATH="${LLVM_CURRENT}/bin:\${PATH}"
export LD_LIBRARY_PATH="\${ROOT}/${MULL_COMPAT_REL}:${MULL_CURRENT}/lib\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}"
exec "\${ROOT}/bin/${target_name}" "\$@"
EOF
  chmod +x "${path}"
}

require_file() {
  local path="$1"
  local label="$2"
  [[ -x "${path}" || -f "${path}" ]] || fail "missing required ${label}: ${path}"
}

require_llvm_layout() {
  require_file "${LLVM_ROOT}/bin/clang" "llvm executable"
  [[ -x "${LLVM_ROOT}/bin/clang++" ]] || fail "missing required llvm executable: ${LLVM_ROOT}/bin/clang++"
  [[ -x "${LLVM_ROOT}/bin/llvm-config" ]] || fail "missing required llvm executable: ${LLVM_ROOT}/bin/llvm-config"
}

require_mull_layout() {
  [[ -x "${MULL_ROOT}/bin/mull-runner-${MULL_LLVM_MAJOR}" ]] || fail \
    "missing required mull executable: ${MULL_ROOT}/bin/mull-runner-${MULL_LLVM_MAJOR}"
  [[ -x "${MULL_ROOT}/bin/mull-reporter-${MULL_LLVM_MAJOR}" ]] || fail \
    "missing required mull executable: ${MULL_ROOT}/bin/mull-reporter-${MULL_LLVM_MAJOR}"
  [[ -f "${MULL_ROOT}/lib/mull-ir-frontend-${MULL_LLVM_MAJOR}" ]] || fail \
    "missing required mull frontend: ${MULL_ROOT}/lib/mull-ir-frontend-${MULL_LLVM_MAJOR}"
}

ensure_mull_compat_lib() {
  local dest_root="$1"
  mkdir -p "${dest_root}/${MULL_COMPAT_REL}"
  if [[ -n "${MULL_SOURCE_DIR}" ]]; then
    return
  fi
  if find "${dest_root}/${MULL_COMPAT_REL}" -maxdepth 1 -type f -name 'libclang-cpp.so.17*' | grep -q .; then
    return
  fi
  local compat_package
  compat_package="$(download_apt_package "${CLANG_CPP_PACKAGE_NAME}" "${CLANG_CPP_PACKAGE_VERSION}" "${CLANG_CPP_ARCH}")"
  extract_mull_package "${compat_package}" "${dest_root}/compat"
}

refresh_mull_wrappers() {
  local dest_root="$1"
  make_wrapper "${dest_root}/bin/mull-runner" "mull-runner-${MULL_LLVM_MAJOR}"
  make_wrapper "${dest_root}/bin/mull-reporter" "mull-reporter-${MULL_LLVM_MAJOR}"
}

install_llvm() {
  if [[ -x "${LLVM_ROOT}/bin/clang" && -x "${LLVM_ROOT}/bin/clang++" && -x "${LLVM_ROOT}/bin/llvm-config" ]]; then
    ln -sfn "${LLVM_ROOT}" "${LLVM_CURRENT}"
    return
  fi

  rm -rf "${LLVM_ROOT}"
  mkdir -p "${TOOLCHAIN_ROOT}/llvm"
  local temp_root="${LLVM_ROOT}.tmp"
  rm -rf "${temp_root}"
  mkdir -p "${temp_root}"
  if [[ -n "${LLVM_SOURCE_DIR}" ]]; then
    copy_tree "${LLVM_SOURCE_DIR}" "${temp_root}"
  else
    local archive="${DOWNLOAD_ROOT}/${LLVM_ARCHIVE_NAME}"
    download_file "${LLVM_URL}" "${archive}"
    extract_llvm_archive "${archive}" "${temp_root}"
  fi
  mv "${temp_root}" "${LLVM_ROOT}"
  require_llvm_layout
  ln -sfn "${LLVM_ROOT}" "${LLVM_CURRENT}"
}

install_mull() {
  local has_payload=0
  if [[ -x "${MULL_ROOT}/bin/mull-runner-${MULL_LLVM_MAJOR}" &&
        -x "${MULL_ROOT}/bin/mull-reporter-${MULL_LLVM_MAJOR}" &&
        -f "${MULL_ROOT}/lib/mull-ir-frontend-${MULL_LLVM_MAJOR}" ]]; then
    has_payload=1
  fi

  if [[ "${has_payload}" -eq 0 ]]; then
    rm -rf "${MULL_ROOT}"
    mkdir -p "${TOOLCHAIN_ROOT}/mull"
    local temp_root="${MULL_ROOT}.tmp"
    rm -rf "${temp_root}"
    mkdir -p "${temp_root}"
    if [[ -n "${MULL_SOURCE_DIR}" ]]; then
      copy_tree "${MULL_SOURCE_DIR}" "${temp_root}"
    else
      local package="${DOWNLOAD_ROOT}/${MULL_PACKAGE_NAME}"
      download_file "${MULL_URL}" "${package}"
      extract_mull_package "${package}" "${temp_root}"
    fi
    require_file "${temp_root}/bin/mull-runner-${MULL_LLVM_MAJOR}" "mull executable"
    require_file "${temp_root}/bin/mull-reporter-${MULL_LLVM_MAJOR}" "mull executable"
    require_file "${temp_root}/lib/mull-ir-frontend-${MULL_LLVM_MAJOR}" "mull frontend"
    ensure_mull_compat_lib "${temp_root}"
    refresh_mull_wrappers "${temp_root}"
    mv "${temp_root}" "${MULL_ROOT}"
  else
    ensure_mull_compat_lib "${MULL_ROOT}"
    refresh_mull_wrappers "${MULL_ROOT}"
  fi
  require_mull_layout
  if [[ -z "${MULL_SOURCE_DIR}" ]]; then
    require_file "${MULL_ROOT}/${MULL_COMPAT_REL}/libclang-cpp.so.17" "mull compat library"
  fi
  ln -sfn "${MULL_ROOT}" "${MULL_CURRENT}"
}

verify_toolchain() {
  require_llvm_layout
  require_mull_layout
  [[ -x "${LLVM_CURRENT}/bin/clang" ]] || fail "missing stable clang path: ${LLVM_CURRENT}/bin/clang"
  [[ -x "${LLVM_CURRENT}/bin/clang++" ]] || fail "missing stable clang++ path: ${LLVM_CURRENT}/bin/clang++"
  [[ -x "${MULL_CURRENT}/bin/mull-runner" ]] || fail "missing stable mull-runner path: ${MULL_CURRENT}/bin/mull-runner"

  local llvm_runtime_version
  llvm_runtime_version="$("${LLVM_CURRENT}/bin/clang" --version | extract_semver)"
  [[ "${llvm_runtime_version}" == "${LLVM_VERSION}" ]] || fail \
    "llvm version mismatch: expected ${LLVM_VERSION}, got ${llvm_runtime_version:-unknown}"

  local mull_runtime_version
  mull_runtime_version="$("${MULL_CURRENT}/bin/mull-runner" --version | extract_semver)"
  [[ "${mull_runtime_version}" == "${MULL_VERSION}" ]] || fail \
    "mull version mismatch: expected ${MULL_VERSION}, got ${mull_runtime_version:-unknown}"

  [[ "${LLVM_MAJOR}" == "${MULL_LLVM_MAJOR}" ]] || fail \
    "llvm/mull major mismatch: llvm=${LLVM_MAJOR} mull=${MULL_LLVM_MAJOR}"
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
install_llvm
install_mull
verify_toolchain

if ((PRINT_PATHS)); then
  echo "toolchain_root=${TOOLCHAIN_ROOT}"
  echo "llvm_root=${LLVM_CURRENT}"
  echo "mull_root=${MULL_CURRENT}"
  echo "clang=${LLVM_CURRENT}/bin/clang"
  echo "clangxx=${LLVM_CURRENT}/bin/clang++"
  echo "llvm_config=${LLVM_CURRENT}/bin/llvm-config"
  echo "mull_runner=${MULL_CURRENT}/bin/mull-runner"
  echo "llvm_version=${LLVM_VERSION}"
  echo "mull_version=${MULL_VERSION}"
  echo "mull_llvm_major=${MULL_LLVM_MAJOR}"
fi
