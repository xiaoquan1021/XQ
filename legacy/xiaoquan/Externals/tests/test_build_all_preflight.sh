#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmpdir="$(mktemp -d)"
trap 'rm -rf "${tmpdir}"' EXIT

workdir="${tmpdir}/Externals"
mkdir -p "${workdir}"
cp "${repo_root}/build_all.sh" "${workdir}/"
cp "${repo_root}/env_variables.sh" "${workdir}/"
cp "${repo_root}/externals.manifest" "${workdir}/"
mkdir -p "${workdir}/scripts"
cp "${repo_root}/scripts/"*.sh "${workdir}/scripts/"

set +e
output="$(cd "${workdir}" && bash build_all.sh xq 2>&1)"
status=$?
set -e

if [ "${status}" -eq 0 ]; then
  echo "expected build_all.sh xq to fail when sources are missing" >&2
  exit 1
fi

grep -q 'Missing source: src/qt-everywhere-src-6.7.0' <<<"${output}"
grep -q 'Run: bash scripts/fetch_sources.sh' <<<"${output}"
test ! -d "${workdir}/install"
test ! -d "${workdir}/output"

set +e
default_output="$(cd "${workdir}" && bash build_all.sh 2>&1)"
default_status=$?
set -e

if [ "${default_status}" -eq 0 ]; then
  echo "expected build_all.sh to default to xq and fail when sources are missing" >&2
  exit 1
fi

grep -q 'Missing source: src/qt-everywhere-src-6.7.0' <<<"${default_output}"
grep -q 'Run: bash scripts/fetch_sources.sh' <<<"${default_output}"
test ! -d "${workdir}/install"
test ! -d "${workdir}/output"
