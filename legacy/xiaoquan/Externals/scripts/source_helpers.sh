#!/usr/bin/env bash

require_source_dir() {
    local source_dir="$1"
    local dependency_name="$2"

    if [ ! -d "$source_dir" ]; then
        echo "[Externals][ERROR] Missing ${dependency_name} source: $source_dir" >&2
        echo "[Externals][ERROR] Run: bash scripts/fetch_sources.sh ${dependency_name}" >&2
        exit 3
    fi
}
