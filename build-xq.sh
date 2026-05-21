#!/usr/bin/env bash
set -euo pipefail

XQ_ROOT="${XQ_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build}"
EXTERNALS_ROOT="${XQ_EXTERNALS_ROOT:-${HOME}/Externals}"
EXTERNALS_INSTALL="${EXTERNALS_ROOT}/install"
BUILD_TYPE="${XQ_BUILD_TYPE:-Release}"
JOBS="${XQ_BUILD_JOBS:-$(nproc)}"

usage() {
  cat <<'EOF'
Usage: ./build-xq.sh [clean] [configure] [build] [test] [run-check]

Default with no arguments: configure build test.

Commands:
  clean       Remove the build directory before configuring.
  configure   Configure CMake.
  build       Build all targets.
  test        Run ctest with the XQ runtime library path.
  run-check   Start XQ for 10 seconds and fail only on immediate startup error.

Environment:
  XQ_BUILD_DIR=/path/to/build
  XQ_EXTERNALS_ROOT=/path/to/Externals
  XQ_BUILD_TYPE=Release|Debug|RelWithDebInfo|MinSizeRel
  XQ_BUILD_JOBS=4
EOF
}

runtime_path() {
  printf '%s' \
    "${BUILD_DIR}/lib:${BUILD_DIR}/lib/plugins:${BUILD_DIR}/bin:"\
"${EXTERNALS_INSTALL}/python-3.11.0/lib:"\
"${EXTERNALS_INSTALL}/hdf5-1.14.3/lib:${EXTERNALS_INSTALL}/hdf5-1.12.2/lib:"\
"${EXTERNALS_INSTALL}/gdcm-3.0.10/lib:${EXTERNALS_INSTALL}/vtk-9.3.0/lib:"\
"${EXTERNALS_INSTALL}/itk-5.4.0/lib:${EXTERNALS_INSTALL}/qt-6.7.0/lib:"\
"${EXTERNALS_INSTALL}/mitk-2024.06/lib:${EXTERNALS_INSTALL}/opencascade-7.6.0/lib:"\
"${EXTERNALS_INSTALL}/freetype-2.13.0/lib:"\
"${EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build/lib:"\
"${EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build/lib/plugins:"\
"${EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/lib:"\
"${EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/src/CTK-build/CTK-build/bin"
}

require_path() {
  if [ ! -e "$1" ]; then
    echo "[XQ][ERROR] Required path missing: $1" >&2
    exit 3
  fi
}

check_dependencies() {
  require_path "${XQ_ROOT}/CMakeLists.txt"
  require_path "${EXTERNALS_INSTALL}/qt-6.7.0/lib/cmake/Qt6/Qt6Config.cmake"
  require_path "${EXTERNALS_INSTALL}/qt-6.7.0/lib/cmake/Qt6GuiTools/Qt6GuiToolsConfig.cmake"
  require_path "${EXTERNALS_INSTALL}/python-3.11.0/bin/python3"
  require_path "${EXTERNALS_INSTALL}/python-3.11.0/lib/libpython3.11.so"
  require_path "${EXTERNALS_INSTALL}/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake"
  require_path "${EXTERNALS_INSTALL}/itk-5.4.0/lib/cmake/ITK-5.4/ITKConfig.cmake"
  require_path "${EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build/MITKConfig.cmake"
}

configure() {
  check_dependencies
  export LD_LIBRARY_PATH="$(runtime_path):${LD_LIBRARY_PATH:-}"
  cmake -S "${XQ_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DXQ_EXTERNALS_ROOT="${EXTERNALS_ROOT}" \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install"
}

build() {
  cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j"${JOBS}"
}

test_xq() {
  export LD_LIBRARY_PATH="$(runtime_path):${LD_LIBRARY_PATH:-}"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
}

run_check() {
  local log="/tmp/xq_run_check.log"
  export LD_LIBRARY_PATH="$(runtime_path):${LD_LIBRARY_PATH:-}"
  export QT_PLUGIN_PATH="${EXTERNALS_INSTALL}/qt-6.7.0/plugins"
  rm -f "${log}"
  set +e
  timeout 10s "${BUILD_DIR}/bin/XQ" >"${log}" 2>&1
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

if [ "$#" -eq 0 ]; then
  set -- configure build test
fi

for command in "$@"; do
  case "${command}" in
    clean)
      rm -rf "${BUILD_DIR}"
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
