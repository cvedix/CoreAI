#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

# Same source default as run_ppe_video.sh — keep the two in sync. The output
# name follows whichever video is actually used, so runs on different footage
# neither collide nor overwrite (the sample refuses to overwrite an output).
default_video="${CVEDIX_VIDEO:-data/videos/20260923090956898_B34B3BKPSFABD4F_L_0_L0120914152000.mp4}"
video_path=""
expect_video=false
for arg in "$@"; do
    if [[ "$expect_video" == true ]]; then
        video_path="$arg"
        expect_video=false
    elif [[ "$arg" == "--video" ]]; then
        expect_video=true
    fi
done
[[ -n "$video_path" ]] || video_path="$default_video"
video_stem="$(basename "$video_path")"
video_stem="${video_stem%.*}"

# Arguments at the end can override the default video/model/output.
exec bash "$repo_dir/scripts/run_ppe_video.sh" \
    --video "$video_path" \
    --person-model data/models/yolov11-cetection_fp16.engine \
    --tracking bytetrack --analysis-board 1 \
    --output "output/ppe/${video_stem}_safety_$(date +%Y%m%d_%H%M%S).mp4" "$@"
