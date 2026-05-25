#!/usr/bin/env bash

xq_env_repo_root() {
  if [ -n "${XQ_ROOT:-}" ]; then
    cd "${XQ_ROOT}" && pwd
    return
  fi

  cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
}

xq_env_required_files() {
  local root="$1"
  printf '%s\n' \
    "${root}/install/qt-6.7.0/lib/cmake/Qt6/Qt6Config.cmake" \
    "${root}/install/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake" \
    "${root}/install/itk-5.4.0/lib/cmake/ITK-5.4/ITKConfig.cmake" \
    "${root}/src/MITK-2024.06/build/MITK-build/MITKConfig.cmake"
}

xq_env_is_valid_externals_root() {
  local root="$1"
  local required

  [ -n "${root}" ] || return 1
  for required in $(xq_env_required_files "${root}"); do
    [ -f "${required}" ] || return 1
  done
}

xq_env_candidate_roots() {
  local repo_root="$1"

  if [ -n "${XQ_EXTERNALS_ROOT:-}" ]; then
    printf '%s\n' "${XQ_EXTERNALS_ROOT}"
    return
  fi

  printf '%s\n' \
    "${repo_root}/../svExternals" \
    "${repo_root}/../External" \
    "${repo_root}/../Externals"

  if [ -n "${HOME:-}" ]; then
    printf '%s\n' \
      "${HOME}/svExternals" \
      "${HOME}/External" \
      "${HOME}/Externals" \
      "${HOME}/Simvascular/Externals"
  fi
}

xq_env_print_missing_required_files() {
  local root="$1"
  local required

  for required in $(xq_env_required_files "${root}"); do
    if [ ! -f "${required}" ]; then
      echo "  - ${required}" >&2
    fi
  done
}

xq_env_resolve_externals_root() {
  local repo_root="$1"
  local candidate
  local resolved
  local candidates=()

  while IFS= read -r candidate; do
    [ -n "${candidate}" ] || continue
    candidates+=("${candidate}")
    if xq_env_is_valid_externals_root "${candidate}"; then
      cd "${candidate}" && pwd
      return
    fi
  done < <(xq_env_candidate_roots "${repo_root}")

  echo "[XQ][ERROR] Missing required external dependency files." >&2
  if [ -n "${XQ_EXTERNALS_ROOT:-}" ]; then
    echo "[XQ][ERROR] XQ_EXTERNALS_ROOT was set to: ${XQ_EXTERNALS_ROOT}" >&2
    echo "[XQ][ERROR] Missing files:" >&2
    xq_env_print_missing_required_files "${XQ_EXTERNALS_ROOT}"
  else
    echo "[XQ][ERROR] Checked candidate roots:" >&2
    for resolved in "${candidates[@]}"; do
      echo "  - ${resolved}" >&2
    done
    echo "[XQ][ERROR] Expected files under a valid root:" >&2
    xq_env_print_missing_required_files "<externals-root>"
  fi
  echo "[XQ][ERROR] Fix: XQ_EXTERNALS_ROOT=/path/to/svExternals ./build-xq.sh configure" >&2
  return 3
}

xq_env_init() {
  local resolved_root

  XQ_ROOT="$(xq_env_repo_root)"
  export XQ_ROOT

  resolved_root="$(xq_env_resolve_externals_root "${XQ_ROOT}")" || return $?
  XQ_EXTERNALS_ROOT="${resolved_root}"
  export XQ_EXTERNALS_ROOT

  XQ_EXTERNALS_INSTALL="${XQ_EXTERNALS_ROOT}/install"
  XQ_MITK_BUILD="${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build"
  export XQ_EXTERNALS_INSTALL XQ_MITK_BUILD
}

xq_env_runtime_path() {
  local build_dir="$1"

  printf '%s' \
    "${build_dir}/lib:${build_dir}/lib/plugins:${build_dir}/bin:"\
"${XQ_EXTERNALS_INSTALL}/python-3.11.0/lib:"\
"${XQ_EXTERNALS_INSTALL}/hdf5-1.14.3/lib:${XQ_EXTERNALS_INSTALL}/hdf5-1.12.2/lib:"\
"${XQ_EXTERNALS_INSTALL}/gdcm-3.0.10/lib:${XQ_EXTERNALS_INSTALL}/vtk-9.3.0/lib:"\
"${XQ_EXTERNALS_INSTALL}/itk-5.4.0/lib:${XQ_EXTERNALS_INSTALL}/qt-6.7.0/lib:"\
"${XQ_EXTERNALS_INSTALL}/mitk-2024.06/lib:${XQ_EXTERNALS_INSTALL}/opencascade-7.6.0/lib:"\
"${XQ_EXTERNALS_INSTALL}/freetype-2.13.0/lib:"\
"${XQ_MITK_BUILD}/lib:${XQ_MITK_BUILD}/lib/plugins:"\
"${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/lib:"\
"${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/ep/src/CTK-build/CTK-build/bin"
}

xq_env_apply_runtime_paths() {
  local build_dir="$1"

  export LD_LIBRARY_PATH="$(xq_env_runtime_path "${build_dir}"):${LD_LIBRARY_PATH:-}"
  export QT_PLUGIN_PATH="${XQ_EXTERNALS_INSTALL}/qt-6.7.0/plugins"
}

xq_env_apply_qt_runtime_defaults() {
  if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=xcb
  fi

  if [ "${QT_QPA_PLATFORM:-}" = "xcb" ]; then
    export QT_XCB_GL_INTEGRATION="${QT_XCB_GL_INTEGRATION:-xcb_glx}"
    export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
  fi
}
