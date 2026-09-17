#!/usr/bin/env python3
"""GPU integration test: parallel PPE/person pipeline and decoded xyxy output.

Requires local engines/video, ffprobe, numpy, OpenCV and onnxruntime.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile

import cv2
import numpy as np
import onnxruntime as ort

ROOT = Path(__file__).resolve().parents[1]


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    args = parser.parse_args()
    env = dict(os.environ)
    env['LD_LIBRARY_PATH'] = str(args.build_dir.resolve() / 'libs') + ':' + env.get('LD_LIBRARY_PATH', '')
    with tempfile.TemporaryDirectory(prefix='ppe-safety-') as tmp:
        output = Path(tmp) / 'safety.mp4'
        result = subprocess.run([
            str(args.build_dir.resolve() / 'bin/ppe_video_sample'),
            '--person-model', 'data/models/yolov11-cetection_fp16.engine',
            '--tracking', 'bytetrack', '--analysis-board', '1',
            '--max-frames', '12', '--output', str(output)],
            cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
        check(result.returncode == 0, result.stdout + result.stderr)
        check('Mode: decoded-xyxy' in result.stdout, 'Decoded TensorRT path was not used')
        check('Done: 12 frames' in result.stdout, 'Incomplete pipeline')
        probe = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0',
            '-show_entries', 'stream=nb_read_frames,width,height', '-of', 'json', str(output)], text=True))['streams'][0]
        check(int(probe['nb_read_frames']) == 12, 'Dropped output frames')
        board_png = output.with_name('safety_board.png')
        board_mp4 = output.with_name('safety_board.mp4')
        check(cv2.imread(str(board_png)) is not None, 'Missing board PNG')
        board_probe = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0',
            '-show_entries', 'stream=nb_read_frames', '-of', 'json', str(board_mp4)], text=True))['streams'][0]
        check(int(board_probe['nb_read_frames']) == 12, 'Missing analysis board frames')
        with output.with_suffix('.csv').open() as f:
            detections = list(csv.DictReader(f))
        with output.with_name('safety_safety.csv').open() as f:
            states = list(csv.DictReader(f))
        check({r['class_id'] for r in detections} == {'0', '1', '100'}, 'Unexpected class mapping')
        signatures = [(r['frame_index'], r['class_id'], r['x'], r['y'], r['width'], r['height'])
                      for r in detections]
        check(len(signatures) == len(set(signatures)), 'Duplicate detections leaked between branches')
        people = [r for r in detections if r['class_id'] == '100']
        check(all(r['track_id'] == '-1' for r in detections if r['class_id'] != '100'),
              'PPE items must not enter the person tracker')
        frame_ids = [{int(r['track_id']) for r in people if int(r['frame_index']) == i and int(r['track_id']) >= 0}
                     for i in range(12)]
        check(len(set.intersection(*frame_ids)) >= 3, 'Expected at least 3 continuous person tracks')
        for i in range(12):
            tracked = [r['track_id'] for r in people if int(r['frame_index']) == i and int(r['track_id']) >= 0]
            check(len(tracked) == len(set(tracked)), 'Duplicate person ID in a frame')
        check(len(states) == len(people) > 0, 'Every person needs one assessment')
        check({int(r['frame_index']) for r in states} == set(range(12)), 'Missing person frame')
        for p, s in zip(people, states):
            for key in ['frame_index', 'source_timestamp_ms', 'confidence', 'x', 'y', 'width', 'height', 'track_id']:
                check(p[key] == s[key], f'Misaligned safety row: {key}')
            expected = {('1', '1'): 'ppe:ok', ('0', '1'): 'ppe:missing_helmet',
                        ('1', '0'): 'ppe:missing_vest', ('0', '0'): 'ppe:missing_helmet_and_vest'}
            check(s['status'] == expected[(s['has_helmet'], s['has_vest'])], 'Wrong safety status')
        # Independent ONNX reference catches swapped xyxy/cxcywh, padding,
        # class offset, confidence decoding, and NMS mistakes in the TRT plugin.
        options = ort.SessionOptions(); options.intra_op_num_threads = 4
        session = ort.InferenceSession(str(ROOT / 'data/models/yolov11-cetection.onnx'),
                                      sess_options=options, providers=['CPUExecutionProvider'])
        cap = cv2.VideoCapture(str(ROOT / 'data/videos/16.22.09.mp4'))
        for idx in range(3):
            ok, frame = cap.read()
            check(ok, 'Cannot read reference frame')
            h, w = frame.shape[:2]; scale = min(640 / w, 640 / h)
            sw, sh = round(w * scale), round(h * scale)
            px, py = (640 - sw) // 2, (640 - sh) // 2
            canvas = np.full((640, 640, 3), 114, dtype=np.uint8)
            canvas[py:py + sh, px:px + sw] = cv2.resize(frame, (sw, sh))
            inp = np.ascontiguousarray(canvas[:, :, ::-1].transpose(2, 0, 1)[None], dtype=np.float32) / 255
            out = dict(zip([o.name for o in session.get_outputs()],
                           session.run(None, {session.get_inputs()[0].name: inp})))
            mask = (out['scores'][0] >= .35) & (out['class_idx'][0] == 0)
            boxes = out['boxes'][0, mask].copy(); scores = out['scores'][0, mask]
            boxes[:, [0, 2]] = np.clip((boxes[:, [0, 2]] - px) / scale, 0, w)
            boxes[:, [1, 3]] = np.clip((boxes[:, [1, 3]] - py) / scale, 0, h)
            boxes[:, 2:] -= boxes[:, :2]
            keep = cv2.dnn.NMSBoxes(boxes.tolist(), scores.tolist(), .35, .45)
            actual = [r for r in people if int(r['frame_index']) == idx]
            check(len(actual) == len(keep) > 0, 'Person count differs from ONNX')
            for k in np.asarray(keep).reshape(-1):
                ref = boxes[k]
                match = min(actual, key=lambda r: sum(abs(float(r[key]) - ref[j])
                            for j, key in enumerate(['x', 'y', 'width', 'height'])))
                check(max(abs(float(match[key]) - ref[j]) for j, key in
                          enumerate(['x', 'y', 'width', 'height'])) <= 5, 'Decoded person box differs from ONNX')
                check(abs(float(match['confidence']) - scores[k]) <= .03, 'Person score differs from ONNX')
                actual.remove(match)
        cap.release()
        print(f'PASS: 12 merged/board frames, {len(states)} person assessments, stable tracks; decoded boxes match ONNX on 3 frames')


if __name__ == '__main__':
    main()
