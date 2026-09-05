#!/bin/bash

# halt on error
set -e

usage() {
    cat <<'EOF'
Usage: bash build_all.sh [profile]

Profiles:
  xq        Build the Ubuntu dependency stack expected by XQ.
  help      Show this help.

Without a profile, the xq profile is used.
EOF
}

source env_variables.sh

MANIFEST="${EXTERNALS_MANIFEST:-$ROOT_DIR/externals.manifest}"

check_xq_sources() {
    if [ ! -f "$MANIFEST" ]; then
        echo "[Externals][ERROR] Manifest not found: $MANIFEST" >&2
        exit 3
    fi

    missing=0
    while IFS='|' read -r key name version repository official_url target_path order
    do
        case "$key" in
            ''|\#*)
                continue
                ;;
        esac

        if [ ! -d "$ROOT_DIR/$target_path" ]; then
            echo "[Externals][ERROR] Missing source: $target_path" >&2
            missing=1
        fi
    done < "$MANIFEST"

    if [ "$missing" -ne 0 ]; then
        echo "[Externals][ERROR] XQ source mirror is incomplete." >&2
        echo "[Externals][ERROR] Run: bash scripts/fetch_sources.sh" >&2
        exit 3
    fi
}

PROFILE="${1:-xq}"
case "$PROFILE" in
    xq)
        check_xq_sources
        ;;
    help|-h|--help)
        usage
        exit 0
        ;;
    *)
        echo "[Externals][ERROR] Unknown profile: $PROFILE" >&2
        usage >&2
        exit 2
        ;;
esac

# create directories
mkdir -p $SRC_DIR
mkdir -p $INSTALL_DIR
mkdir -p output

source scripts/build_qt.sh > output/qt.out
source scripts/build_hdf5.sh > output/hdf5.out
source scripts/build_tinyxml2.sh > output/tinyxml2.out
source scripts/build_python.sh > output/python.out
source scripts/build_freetype.sh > output/freetype.out
source scripts/build_swig.sh > output/swig.out
source scripts/build_mmg.sh > output/mmg.out
source scripts/build_gdcm.sh > output/gdcm.out
source scripts/build_vtk.sh > output/vtk.out
source scripts/build_itk.sh > output/itk.out
source scripts/build_opencascade.sh > output/opencascade.out
source scripts/build_mitk.sh
