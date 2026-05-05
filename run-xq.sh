#!/usr/bin/env bash
set -euo pipefail

XQ_ROOT="${XQ_ROOT:-/home/xiaoquan/XQ}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build}"
EXTERNALS_ROOT="${XQ_EXTERNALS_ROOT:-/home/xiaoquan/Externals}"
EXTERNALS_INSTALL="${EXTERNALS_ROOT}/install"
MITK_BUILD="${EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build"
XQ_BIN="${BUILD_DIR}/bin/XQ"

for p in \
  "${XQ_BIN}" \
  "${BUILD_DIR}/lib" \
  "${BUILD_DIR}/lib/plugins" \
  "${EXTERNALS_INSTALL}/python-3.11.0/lib" \
  "${EXTERNALS_INSTALL}/hdf5-1.14.3/lib" \
  "${EXTERNALS_INSTALL}/vtk-9.3.0/lib" \
  "${EXTERNALS_INSTALL}/itk-5.4.0/lib" \
  "${EXTERNALS_INSTALL}/qt-6.7.0/lib" \
  "${EXTERNALS_INSTALL}/opencascade-7.6.0/lib" \
  "${EXTERNALS_INSTALL}/gdcm-3.0.10/lib" \
  "${MITK_BUILD}/lib" \
  "${MITK_BUILD}/lib/plugins"; do
  if [ ! -e "$p" ]; then
    echo "[XQ][ERROR] Required runtime path missing: $p" >&2
    exit 3
  fi
done

export LD_LIBRARY_PATH="${BUILD_DIR}/lib:${BUILD_DIR}/lib/plugins:${EXTERNALS_INSTALL}/python-3.11.0/lib:${EXTERNALS_INSTALL}/hdf5-1.14.3/lib:${EXTERNALS_INSTALL}/vtk-9.3.0/lib:${EXTERNALS_INSTALL}/itk-5.4.0/lib:${EXTERNALS_INSTALL}/qt-6.7.0/lib:${EXTERNALS_INSTALL}/opencascade-7.6.0/lib:${EXTERNALS_INSTALL}/gdcm-3.0.10/lib:${MITK_BUILD}/lib:${MITK_BUILD}/lib/plugins:${EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/lib:${EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/src/CTK-build/CTK-build/bin:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${EXTERNALS_INSTALL}/qt-6.7.0/plugins"

exec "${XQ_BIN}" "$@"
