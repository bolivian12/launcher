#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${EBALIA_BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/ebalia-build}
cd /tmp
nix develop "$project_dir" --command bash -c 'cmake -S "$1" -B "$2" -DCMAKE_BUILD_TYPE=Release && cmake --build "$2" --parallel 4 && ctest --test-dir "$2" --output-on-failure' _ "$project_dir" "$build_dir"
