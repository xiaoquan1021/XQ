#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/xq-env.sh
source "${SCRIPT_DIR}/scripts/xq-env.sh"

XQ_ROOT="${XQ_ROOT:-${SCRIPT_DIR}}"
BUILD_DIR="${XQ_BUILD_DIR:-${XQ_ROOT}/build/linux-release}"

usage() {
  cat <<'EOF'
Usage: ./run-xq.sh [--build-dir <dir>] [--externals-root <dir>] [--] [xq args...]

Options:
  --build-dir <dir>       XQ build directory. Defaults to build/linux-release.
  --externals-root <dir>  Externals repository root.
  --help                  Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --externals-root)
      export XQ_EXTERNALS_ROOT="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

xq_env_init
xq_env_apply_runtime_paths "${BUILD_DIR}"
xq_env_apply_qt_runtime_defaults

XQ_BIN="${BUILD_DIR}/bin/XQ"
if [ ! -x "${XQ_BIN}" ]; then
  echo "[XQ][ERROR] XQ executable not found or not executable: ${XQ_BIN}" >&2
  exit 3
fi

exec "${XQ_BIN}" "$@"
