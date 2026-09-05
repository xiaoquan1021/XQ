#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmpdir="$(mktemp -d)"
trap 'rm -rf "${tmpdir}"' EXIT

manifest="${tmpdir}/externals.manifest"
cat >"${manifest}" <<'MANIFEST'
# key|name|version|repository|official_url|target_path|order
Qt|Qt|6.7.0|file://__REPO__/Qt|https://example.invalid/qt.tar.xz|src/qt-everywhere-src-6.7.0|10
MITK|MITK|2024.06|file://__REPO__/MITK|https://example.invalid/mitk.tar.gz|src/MITK-2024.06|20
MANIFEST

source_root="${tmpdir}/sources"
mkdir -p "${source_root}/Qt" "${source_root}/MITK" "${source_root}/Plain"
git -C "${source_root}/Qt" init -q
printf 'qt source\n' >"${source_root}/Qt/README.md"
git -C "${source_root}/Qt" add README.md
git -C "${source_root}/Qt" -c user.name=test -c user.email=test@example.invalid commit -q -m init
git -C "${source_root}/MITK" init -q
printf 'mitk source\n' >"${source_root}/MITK/README.md"
git -C "${source_root}/MITK" add README.md
git -C "${source_root}/MITK" -c user.name=test -c user.email=test@example.invalid commit -q -m init
git -C "${source_root}/Plain" init -q
printf 'plain source\n' >"${source_root}/Plain/README.md"
git -C "${source_root}/Plain" add README.md
git -C "${source_root}/Plain" -c user.name=test -c user.email=test@example.invalid commit -q -m init
sed -i "s#__REPO__#${source_root}#g" "${manifest}"

workdir="${tmpdir}/work"
mkdir -p "${workdir}"

dry_run_output="$(
  EXTERNALS_MANIFEST="${manifest}" \
  EXTERNALS_ROOT="${workdir}" \
  bash "${repo_root}/scripts/fetch_sources.sh" --dry-run Qt
)"

grep -q 'clone Qt -> src/qt-everywhere-src-6.7.0' <<<"${dry_run_output}"
test ! -e "${workdir}/src/qt-everywhere-src-6.7.0"

EXTERNALS_MANIFEST="${manifest}" \
EXTERNALS_ROOT="${workdir}" \
bash "${repo_root}/scripts/fetch_sources.sh" Qt

test -f "${workdir}/src/qt-everywhere-src-6.7.0/README.md"

printf 'qt source update\n' >>"${source_root}/Qt/README.md"
git -C "${source_root}/Qt" add README.md
git -C "${source_root}/Qt" -c user.name=test -c user.email=test@example.invalid commit -q -m update

EXTERNALS_MANIFEST="${manifest}" \
EXTERNALS_ROOT="${workdir}" \
bash "${repo_root}/scripts/fetch_sources.sh" Qt

grep -q 'qt source update' "${workdir}/src/qt-everywhere-src-6.7.0/README.md"

mkdir -p "${workdir}/src/not-a-git-tree"
cat >>"${manifest}" <<'MANIFEST'
Plain|Plain|1.0|file://__REPO__/Plain|https://example.invalid/plain.tar.gz|src/not-a-git-tree|30
MANIFEST
sed -i "s#__REPO__#${source_root}#g" "${manifest}"
if EXTERNALS_MANIFEST="${manifest}" EXTERNALS_ROOT="${workdir}" bash "${repo_root}/scripts/fetch_sources.sh" Plain >/tmp/fetch_sources_existing.out 2>&1; then
  echo "expected fetch_sources.sh to reject an existing non-git source without --force" >&2
  exit 1
fi
grep -q 'already exists' /tmp/fetch_sources_existing.out

EXTERNALS_MANIFEST="${manifest}" \
EXTERNALS_ROOT="${workdir}" \
bash "${repo_root}/scripts/fetch_sources.sh" --force all

test -f "${workdir}/src/MITK-2024.06/README.md"
