#!/usr/bin/env python3
"""Opt-in regression for the supplied PPE engine/video (requires NVIDIA GPU).

Runs the native node pipeline with both backends. In particular, the early
frames exercise equal FP16 scores on adjacent anchors: unstable NMS ordering
used to select different boxes, displaced by up to 8 pixels.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(build_dir, backend, directory):
    output = directory / f"{backend}.mp4"
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = str(build_dir / "libs") + (
        ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
    result = subprocess.run(
        [str(build_dir / "bin/ppe_video_sample"), "--backend", backend,
         "--max-frames", "30", "--output", str(output)],
        cwd=ROOT, env=env, text=True, capture_output=True, timeout=180,
    )
    check(result.returncode == 0, result.stdout + result.stderr)
    expected = "TensorRT / NVIDIA GPU" if backend == "tensorrt" else "OpenCV DNN / CPU"
    check("Backend: " + expected in result.stdout, "Wrong active backend")
    check("Done: 30 frames" in result.stdout, "Pipeline did not complete 30 frames")
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-count_frames",
         "-show_entries", "stream=nb_read_frames,width,height", "-of", "json", str(output)],
        text=True, capture_output=True, check=True, timeout=60,
    )
    video = json.loads(probe.stdout)["streams"][0]
    check(int(video["nb_read_frames"]) == 30, "Output dropped frames")
    with output.with_suffix(".csv").open() as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        x, y, w, h = (int(row[k]) for k in ("x", "y", "width", "height"))
        check(w > 0 and h > 0 and 0 <= x <= video["width"] - w
              and 0 <= y <= video["height"] - h, "Box outside video")
        check(.25 <= float(row["confidence"]) <= 1, "Invalid confidence")
    check({r["class_id"] for r in rows} == {"0", "1"}, "Expected both PPE classes")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ppe-regression-") as tmp:
        cpu = run(args.build_dir.resolve(), "onnx", Path(tmp))
        gpu = run(args.build_dir.resolve(), "tensorrt", Path(tmp))
    check(len(cpu) == len(gpu) > 0, "Detection count changed between backends")
    remaining = list(gpu)
    largest_delta = 0
    for ref in cpu:
        candidates = [r for r in remaining if r["frame_index"] == ref["frame_index"]
                      and r["class_id"] == ref["class_id"]]
        check(bool(candidates), f"Missing detection on frame {ref['frame_index']}")
        keys = ("x", "y", "width", "height")
        match = min(candidates, key=lambda r: sum(abs(int(r[k]) - int(ref[k])) for k in keys))
        delta = max(abs(int(match[k]) - int(ref[k])) for k in keys)
        check(delta <= 2, f"PPE box mismatch (possibly NMS score tie): {ref} vs {match}")
        check(abs(float(match["confidence"]) - float(ref["confidence"])) < .01,
              f"Confidence mismatch: {ref} vs {match}")
        remaining.remove(match)
        largest_delta = max(largest_delta, delta)
    check(not remaining, "Unexpected TensorRT detections")
    print(f"PASS: 30 frames/backend, {len(cpu)} matching detections, max box delta {largest_delta}px")


if __name__ == "__main__":
    main()
