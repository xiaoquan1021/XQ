#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${EXTERNALS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
MANIFEST="${EXTERNALS_MANIFEST:-${ROOT_DIR}/externals.manifest}"
DRY_RUN=0
FORCE=0
TARGET="all"

usage() {
  cat <<'EOF'
Usage: bash scripts/fetch_sources.sh [options] [all|Dependency]

Options:
  --dry-run   Print clone/update actions without changing files.
  --force     Remove an existing target source directory before cloning.
  -h, --help  Show this help.

Examples:
  bash scripts/fetch_sources.sh
  bash scripts/fetch_sources.sh Qt
  bash scripts/fetch_sources.sh --dry-run all
  bash scripts/fetch_sources.sh --force MITK
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --dry-run)
      DRY_RUN=1
      ;;
    --force)
      FORCE=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    all)
      TARGET="all"
      ;;
    *)
      TARGET="$1"
      ;;
  esac
  shift
done

if [ ! -f "${MANIFEST}" ]; then
  echo "[Externals][ERROR] Manifest not found: ${MANIFEST}" >&2
  exit 2
fi

normalize() {
  printf '%s' "$1" | tr '[:upper:]' '[:lower:]'
}

matches_target() {
  local key="$1"
  local name="$2"
  if [ "${TARGET}" = "all" ]; then
    return 0
  fi
  [ "$(normalize "${TARGET}")" = "$(normalize "${key}")" ] || \
    [ "$(normalize "${TARGET}")" = "$(normalize "${name}")" ]
}

clone_source() {
  local key="$1"
  local name="$2"
  local version="$3"
  local repository="$4"
  local official_url="$5"
  local target_path="$6"
  local full_target="${ROOT_DIR}/${target_path}"

  if [ -e "${full_target}" ]; then
    if [ "${FORCE}" -eq 0 ] && git -C "${full_target}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      echo "update ${key} -> ${target_path}"
      echo "  repo: ${repository}"
      echo "  version: ${version}"
      echo "  official: ${official_url}"

      if [ "${DRY_RUN}" -eq 1 ]; then
        return 0
      fi

      git -C "${full_target}" pull --ff-only
      return 0
    fi

    if [ "${FORCE}" -eq 0 ]; then
      echo "[Externals][ERROR] ${target_path} already exists; use --force to replace it." >&2
      return 1
    fi
    if [ "${DRY_RUN}" -eq 1 ]; then
      echo "remove ${target_path}"
    else
      rm -rf "${full_target}"
    fi
  fi

  echo "clone ${key} -> ${target_path}"
  echo "  repo: ${repository}"
  echo "  version: ${version}"
  echo "  official: ${official_url}"

  if [ "${DRY_RUN}" -eq 1 ]; then
    return 0
  fi

  mkdir -p "$(dirname "${full_target}")"
  git clone --depth 1 "${repository}" "${full_target}"
}

matched=0
while IFS='|' read -r key name version repository official_url target_path order; do
  case "${key}" in
    ''|\#*)
      continue
      ;;
  esac
  if matches_target "${key}" "${name}"; then
    matched=1
    clone_source "${key}" "${name}" "${version}" "${repository}" "${official_url}" "${target_path}"
  fi
done <"${MANIFEST}"

if [ "${matched}" -eq 0 ]; then
  echo "[Externals][ERROR] Dependency not found in manifest: ${TARGET}" >&2
  exit 2
fi
