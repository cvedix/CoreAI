#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"
build_dir="${CVEDIX_BUILD_DIR:-$repo_dir/build}"
# Source footage for a plain run. The sample hardcodes the 16.22.09 clip, so the
# script passes the source explicitly; a --video in "$@" comes later on the
# command line and still wins, matching the sample's own argument order.
default_video="${CVEDIX_VIDEO:-data/videos/20260923090956898_B34B3BKPSFABD4F_L_0_L0120914152000.mp4}"
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    echo "Configure the SDK first: cmake -S . -B \"$build_dir\" (see docs/PPE_VIDEO.md)" >&2
    exit 1
fi
cmake --build "$build_dir" --target ppe_video_sample -j "${CVEDIX_BUILD_JOBS:-4}"
# The detector loads the backend plugin by name with dlopen.
export LD_LIBRARY_PATH="$build_dir/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$build_dir/bin/ppe_video_sample" --video "$default_video" "$@"
