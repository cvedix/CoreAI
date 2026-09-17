#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"
build_dir="${CVEDIX_BUILD_DIR:-$repo_dir/build}"
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    echo "Configure the SDK first: cmake -S . -B \"$build_dir\" (see docs/PPE_VIDEO.md)" >&2
    exit 1
fi
cmake --build "$build_dir" --target ppe_video_sample -j "${CVEDIX_BUILD_JOBS:-4}"
# The detector loads the backend plugin by name with dlopen.
export LD_LIBRARY_PATH="$build_dir/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$build_dir/bin/ppe_video_sample" "$@"
