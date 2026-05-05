#!/usr/bin/env bash
set -euo pipefail

XQ_ROOT="${XQ_ROOT:-/home/xiaoquan/XQ}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build}"
EXTERNALS_ROOT="${XQ_EXTERNALS_ROOT:-/home/xiaoquan/Externals}"
EXTERNALS_INSTALL="${EXTERNALS_ROOT}/install"
MITK_BUILD="${EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build"
BUILD_TYPE="${1:-Release}"

if [ "${1:-}" = "clean" ]; then
  echo "[XQ][ERROR] clean build is forbidden by the current repair plan." >&2
  exit 2
fi

case "${1:-Release}" in
  debug) BUILD_TYPE="Debug" ;;
  release) BUILD_TYPE="Release" ;;
  Debug|Release|RelWithDebInfo|MinSizeRel) BUILD_TYPE="${1:-Release}" ;;
  *) BUILD_TYPE="Release" ;;
esac

for p in \
  "${XQ_ROOT}" \
  "${BUILD_DIR}" \
  "${EXTERNALS_INSTALL}/python-3.11.0/lib" \
  "${EXTERNALS_INSTALL}/qt-6.7.0/lib" \
  "${EXTERNALS_INSTALL}/vtk-9.3.0/lib" \
  "${EXTERNALS_INSTALL}/itk-5.4.0/lib" \
  "${EXTERNALS_INSTALL}/mitk-2024.06/lib" \
  "${MITK_BUILD}"; do
  if [ ! -e "$p" ]; then
    echo "[XQ][ERROR] Required path missing: $p" >&2
    exit 3
  fi
done

export LD_LIBRARY_PATH="${EXTERNALS_INSTALL}/python-3.11.0/lib:${EXTERNALS_INSTALL}/qt-6.7.0/lib:${EXTERNALS_INSTALL}/vtk-9.3.0/lib:${EXTERNALS_INSTALL}/itk-5.4.0/lib:${EXTERNALS_INSTALL}/mitk-2024.06/lib:${LD_LIBRARY_PATH:-}"

cmake -S "${XQ_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_PREFIX_PATH="${EXTERNALS_INSTALL}/qt-6.7.0;${EXTERNALS_INSTALL}/vtk-9.3.0;${EXTERNALS_INSTALL}/itk-5.4.0;${EXTERNALS_INSTALL}/mitk-2024.06;${EXTERNALS_INSTALL}/gdcm-3.0.10;${EXTERNALS_INSTALL}/hdf5-1.14.3;${EXTERNALS_INSTALL}/tinyxml2-8.0.0;${EXTERNALS_INSTALL}/freetype-2.13.0" \
  -DMITK_DIR="${MITK_BUILD}" \
  -DVTK_DIR="${EXTERNALS_INSTALL}/vtk-9.3.0/lib/cmake/vtk-9.3" \
  -DITK_DIR="${EXTERNALS_INSTALL}/itk-5.4.0/lib/cmake/ITK-5.4" \
  -DQt6_DIR="${EXTERNALS_INSTALL}/qt-6.7.0/lib/cmake/Qt6" \
  -DOpenCASCADE_DIR="${EXTERNALS_INSTALL}/opencascade-7.6.0/lib/cmake/opencascade" \
  -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j2
