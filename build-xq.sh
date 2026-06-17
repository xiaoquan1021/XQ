#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/xq-env.sh
source "${SCRIPT_DIR}/scripts/xq-env.sh"

XQ_ROOT="${XQ_ROOT:-${SCRIPT_DIR}}"
BUILD_TYPE="${XQ_BUILD_TYPE:-Release}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build/linux-release}"
JOBS="${XQ_BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"
PRESET="${XQ_CMAKE_PRESET:-linux-release}"

usage() {
  cat <<'EOF'
Usage: ./build-xq.sh [commands]

Default with no arguments: configure build test.

Commands:
  doctor      Print resolved Linux build/runtime paths.
  clean       Remove the Linux build directory.
  configure   Configure CMake with the linux-release preset.
  build       Build all targets.
  test        Run ctest with the XQ runtime library path.
  run-check   Start XQ for 10 seconds and fail only on immediate startup error.

Environment:
  XQ_BUILD_DIR=/path/to/build
  XQ_EXTERNALS_ROOT=/path/to/Externals
  XQ_CMAKE_PRESET=linux-release
  XQ_BUILD_TYPE=Release|Debug|RelWithDebInfo|MinSizeRel
  XQ_BUILD_JOBS=4
EOF
}

ensure_linux_environment() {
  xq_env_init
}

configure() {
  ensure_linux_environment
  xq_env_apply_runtime_paths "${BUILD_DIR}"
  xq_env_apply_qt_runtime_defaults

  cmake --preset "${PRESET}" \
    -B "${BUILD_DIR}" \
    -DXQ_EXTERNALS_ROOT="${XQ_EXTERNALS_ROOT}" \
    -DXQ_EXTERNALS_PLATFORM="${XQ_EXTERNALS_PLATFORM:-}" \
    -DXQ_BUILD_LEGACY_BLUEBERRY=ON \
    -DXQ_BUILD_MONOLITH=OFF \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install"
}

build() {
  cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${JOBS}"
}

test_xq() {
  ensure_linux_environment
  xq_env_apply_runtime_paths "${BUILD_DIR}"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
}

run_check() {
  ensure_linux_environment
  xq_env_apply_runtime_paths "${BUILD_DIR}"
  xq_env_apply_qt_runtime_defaults

  local exe="${BUILD_DIR}/bin/XQ"
  local log="${TMPDIR:-/tmp}/xq_run_check.log"
  if [ ! -x "${exe}" ]; then
    echo "[XQ][ERROR] XQ executable not found or not executable: ${exe}" >&2
    exit 3
  fi

  rm -f "${log}"
  set +e
  timeout 10s "${exe}" >"${log}" 2>&1
  local status=$?
  set -e
  sed -n '1,220p' "${log}"
  if [ "${status}" -eq 124 ]; then
    echo "[XQ] run-check passed: XQ stayed alive for 10 seconds."
    return 0
  fi
  if [ "${status}" -eq 0 ]; then
    echo "[XQ] run-check passed: XQ exited normally."
    return 0
  fi
  echo "[XQ][ERROR] run-check failed with exit status ${status}." >&2
  return "${status}"
}

clean() {
  local resolved_build
  local resolved_root

  mkdir -p "$(dirname "${BUILD_DIR}")"
  resolved_build="$(cd "$(dirname "${BUILD_DIR}")" && pwd)/$(basename "${BUILD_DIR}")"
  resolved_root="$(cd "${XQ_ROOT}" && pwd)"
  case "${resolved_build}" in
    "${resolved_root}/build/"*|"${resolved_root}/build")
      rm -rf "${resolved_build}"
      ;;
    *)
      echo "[XQ][ERROR] Refusing to clean build directory outside ${resolved_root}/build: ${resolved_build}" >&2
      exit 3
      ;;
  esac
}

doctor() {
  ensure_linux_environment
  cat <<EOF
[XQ] Root: ${XQ_ROOT}
[XQ] Build: ${BUILD_DIR}
[XQ] Externals: ${XQ_EXTERNALS_ROOT}
[XQ] Externals install: ${XQ_EXTERNALS_INSTALL}
[XQ] MITK build: ${XQ_MITK_BUILD}
[XQ] CMake preset: ${PRESET}
[XQ] Jobs: ${JOBS}
EOF
}

if [ "$#" -eq 0 ]; then
  set -- configure build test
fi

for command in "$@"; do
  case "${command}" in
    doctor)
      doctor
      ;;
    clean)
      clean
      ;;
    configure)
      configure
      ;;
    build)
      build
      ;;
    test)
      test_xq
      ;;
    run-check)
      run_check
      ;;
    help|-h|--help)
      usage
      ;;
    *)
      echo "[XQ][ERROR] Unknown command: ${command}" >&2
      usage >&2
      exit 2
      ;;
  esac
done
