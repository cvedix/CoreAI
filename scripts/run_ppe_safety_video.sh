#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# Arguments at the end can override the default model/output.
exec bash "$repo_dir/scripts/run_ppe_video.sh" \
    --person-model data/models/yolov11-cetection_fp16.engine \
    --tracking bytetrack --analysis-board 1 \
    --output "output/ppe/16.22.09_safety_$(date +%Y%m%d_%H%M%S).mp4" "$@"
