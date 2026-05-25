#!/usr/bin/env bash
set -euo pipefail

XQ_ROOT="${XQ_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build}"
XQ_BIN="${BUILD_DIR}/bin/XQ"

# shellcheck source=scripts/xq-env.sh
source "${XQ_ROOT}/scripts/xq-env.sh"
xq_env_init

for p in \
  "${XQ_BIN}" \
  "${BUILD_DIR}/bin" \
  "${BUILD_DIR}/lib" \
  "${BUILD_DIR}/lib/plugins" \
  "${XQ_EXTERNALS_INSTALL}/python-3.11.0/lib" \
  "${XQ_EXTERNALS_INSTALL}/hdf5-1.14.3/lib" \
  "${XQ_EXTERNALS_INSTALL}/vtk-9.3.0/lib" \
  "${XQ_EXTERNALS_INSTALL}/itk-5.4.0/lib" \
  "${XQ_EXTERNALS_INSTALL}/qt-6.7.0/lib" \
  "${XQ_EXTERNALS_INSTALL}/mitk-2024.06/lib" \
  "${XQ_EXTERNALS_INSTALL}/opencascade-7.6.0/lib" \
  "${XQ_EXTERNALS_INSTALL}/gdcm-3.0.10/lib" \
  "${XQ_MITK_BUILD}/lib" \
  "${XQ_MITK_BUILD}/lib/plugins"; do
  if [ ! -e "$p" ]; then
    echo "[XQ][ERROR] Required runtime path missing: $p" >&2
    exit 3
  fi
done

xq_env_apply_runtime_paths "${BUILD_DIR}"
xq_env_apply_qt_runtime_defaults

exec "${XQ_BIN}" "$@"
