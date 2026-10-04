#!/usr/bin/env python3
"""Run shirt/pants and person TensorRT engines on a video and export reports."""

import argparse
import csv
import json
import time
from pathlib import Path

import cv2
import numpy as np
import pycuda.driver as cuda
import pycuda.autoinit
import tensorrt as trt


class EngineRunner:
    def __init__(self, path):
        self.path = Path(path)
        self.logger = trt.Logger(trt.Logger.ERROR)
        self.runtime = trt.Runtime(self.logger)
        self.engine = self.runtime.deserialize_cuda_engine(self.path.read_bytes())
        if self.engine is None:
            raise RuntimeError(f"Cannot deserialize TensorRT engine: {self.path}")
        self.context = self.engine.create_execution_context()
        self.stream = cuda.Stream()
        self.input_name = None
        self.input_shape = None
        self.input_device = None
        self.output_buffers = {}
        for index in range(self.engine.num_io_tensors):
            name = self.engine.get_tensor_name(index)
            shape = tuple(self.engine.get_tensor_shape(name))
            if self.engine.get_tensor_mode(name) == trt.TensorIOMode.INPUT:
                self.input_name = name
                self.input_shape = shape
                self.input_device = cuda.mem_alloc(int(np.prod(shape)) * np.dtype(np.float32).itemsize)
                self.context.set_tensor_address(name, int(self.input_device))
            else:
                host = cuda.pagelocked_empty(int(np.prod(shape)), np.float32)
                device = cuda.mem_alloc(host.nbytes)
                self.output_buffers[name] = (shape, host, device)
                self.context.set_tensor_address(name, int(device))
        if self.input_shape != (1, 3, 640, 640):
            raise RuntimeError(f"Unexpected input shape for {self.path}: {self.input_shape}")

    def infer(self, tensor):
        cuda.memcpy_htod_async(self.input_device, tensor, self.stream)
        if not self.context.execute_async_v3(stream_handle=self.stream.handle):
            raise RuntimeError(f"TensorRT inference failed: {self.path}")
        results = {}
        for name, (shape, host, device) in self.output_buffers.items():
            cuda.memcpy_dtoh_async(host, device, self.stream)
            results[name] = host.reshape(shape)
        self.stream.synchronize()
        return results


def preprocess(frame):
    height, width = frame.shape[:2]
    scale = min(640 / width, 640 / height)
    scaled_width, scaled_height = round(width * scale), round(height * scale)
    pad_x, pad_y = (640 - scaled_width) // 2, (640 - scaled_height) // 2
    canvas = np.full((640, 640, 3), 114, dtype=np.uint8)
    resized = cv2.resize(frame, (scaled_width, scaled_height))
    canvas[pad_y:pad_y + scaled_height, pad_x:pad_x + scaled_width] = resized
    tensor = np.ascontiguousarray(canvas[:, :, ::-1].transpose(2, 0, 1)[None], dtype=np.float32)
    tensor *= 1.0 / 255.0
    return tensor, scale, pad_x, pad_y


def restore_box(box, scale, pad_x, pad_y, width, height):
    x1 = np.clip((box[0] - pad_x) / scale, 0, width)
    y1 = np.clip((box[1] - pad_y) / scale, 0, height)
    x2 = np.clip((box[2] - pad_x) / scale, 0, width)
    y2 = np.clip((box[3] - pad_y) / scale, 0, height)
    return [float(x1), float(y1), float(x2), float(y2)]


def nms(detections, threshold):
    kept = []
    for class_id in sorted({item[0] for item in detections}):
        group = [item for item in detections if item[0] == class_id]
        boxes = [[item[1][0], item[1][1], item[1][2] - item[1][0], item[1][3] - item[1][1]]
                 for item in group]
        indexes = cv2.dnn.NMSBoxes(boxes, [item[2] for item in group], 0.0, threshold)
        for index in np.asarray(indexes).reshape(-1) if len(indexes) else []:
            kept.append(group[int(index)])
    return kept


def decode_uniform(outputs, scale, pad_x, pad_y, width, height, confidence, nms_threshold):
    output = outputs["output0"][0]
    count = output.shape[1]
    candidates = []
    for index in range(count):
        class_scores = output[4:, index]
        class_id = int(np.argmax(class_scores))
        score = float(class_scores[class_id])
        if score < confidence:
            continue
        cx, cy, box_width, box_height = (float(output[channel, index]) for channel in range(4))
        box = restore_box((cx - box_width / 2, cy - box_height / 2,
                           cx + box_width / 2, cy + box_height / 2),
                          scale, pad_x, pad_y, width, height)
        if box[2] > box[0] and box[3] > box[1]:
            candidates.append((class_id, box, score))
    return nms(candidates, nms_threshold)


def decode_people(outputs, scale, pad_x, pad_y, width, height, confidence, nms_threshold):
    boxes = outputs["boxes"][0]
    scores = outputs["scores"][0]
    classes = outputs["class_idx"][0]
    candidates = []
    for box, score, class_id in zip(boxes, scores, classes):
        if int(class_id) != 0 or float(score) < confidence:
            continue
        restored = restore_box(box, scale, pad_x, pad_y, width, height)
        if restored[2] > restored[0] and restored[3] > restored[1]:
            candidates.append((100, restored, float(score)))
    return nms(candidates, nms_threshold)


def box_iou(a, b):
    x1, y1 = max(a[0], b[0]), max(a[1], b[1])
    x2, y2 = min(a[2], b[2]), min(a[3], b[3])
    intersection = max(0, x2 - x1) * max(0, y2 - y1)
    area_a = max(0, a[2] - a[0]) * max(0, a[3] - a[1])
    area_b = max(0, b[2] - b[0]) * max(0, b[3] - b[1])
    return intersection / (area_a + area_b - intersection + 1e-6)


class IoUTracker:
    def __init__(self, max_age=60, threshold=.25):
        self.max_age = max_age
        self.threshold = threshold
        self.next_id = 1
        self.tracks = {}

    def update(self, people):
        candidates = sorted(
            ((box_iou(track["box"], person[1]), track_id, index)
             for track_id, track in self.tracks.items()
             for index, person in enumerate(people)
             if track["class_id"] == person[0]), reverse=True)
        used_tracks, used_people = set(), set()
        for overlap, track_id, index in candidates:
            if overlap < self.threshold:
                break
            if track_id in used_tracks or index in used_people:
                continue
            self.tracks[track_id] = {"box": people[index][1], "class_id": people[index][0], "age": 0}
            people[index].append(track_id)
            used_tracks.add(track_id)
            used_people.add(index)
        for index, person in enumerate(people):
            if index not in used_people:
                track_id = self.next_id
                self.next_id += 1
                self.tracks[track_id] = {"box": person[1], "class_id": person[0], "age": 0}
                person.append(track_id)
        for track_id in list(self.tracks):
            if track_id not in used_tracks and all(person[-1] != track_id for person in people):
                self.tracks[track_id]["age"] += 1
            if self.tracks[track_id]["age"] > self.max_age:
                del self.tracks[track_id]


def associate_uniform(people, garments):
    assignments = {index: {0: -1, 1: -1} for index in range(len(people))}
    used = set()
    for class_id, target_y, min_y, max_y in ((0, .35, .10, .65), (1, .72, .35, 1.02)):
        candidates = []
        for person_index, person in enumerate(people):
            px1, py1, px2, py2 = person[1]
            pw, ph = px2 - px1, py2 - py1
            region = (px1 - .08 * pw, py1 - .04 * ph, px2 + .08 * pw, py2 + .04 * ph)
            for item_index, garment in enumerate(garments):
                if garment[0] != class_id or item_index in used:
                    continue
                x1, y1, x2, y2 = garment[1]
                area = max(0, x2 - x1) * max(0, y2 - y1)
                if area <= 0:
                    continue
                intersection = max(0, min(region[2], x2) - max(region[0], x1)) * \
                    max(0, min(region[3], y2) - max(region[1], y1))
                coverage = intersection / area
                nx, ny = ((x1 + x2) * .5 - px1) / pw, ((y1 + y2) * .5 - py1) / ph
                if coverage < .6 or nx < -.08 or nx > 1.08 or ny < min_y or ny > max_y:
                    continue
                cost = abs(nx - .5) + abs(ny - target_y) + 1 - coverage
                candidates.append((cost, person_index, item_index))
        assigned_people = set()
        for _, person_index, item_index in sorted(candidates):
            if person_index not in assigned_people:
                assignments[person_index][class_id] = item_index
                assigned_people.add(person_index)
                used.add(item_index)
    return assignments


def draw_box(frame, box, label, color, thickness, text_color=None):
    x1, y1, x2, y2 = [int(round(value)) for value in box]
    cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)
    if text_color is None:
        text_color = color
    font_scale = max(.55, frame.shape[1] / 2200)
    text_size, baseline = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, font_scale, 2)
    top = max(text_size[1] + baseline + 4, y1)
    cv2.rectangle(frame, (x1, top - text_size[1] - baseline - 6),
                  (x1 + text_size[0] + 8, top), (20, 20, 20), cv2.FILLED)
    cv2.putText(frame, label, (x1 + 4, top - baseline - 2), cv2.FONT_HERSHEY_SIMPLEX,
                font_scale, text_color, 2, cv2.LINE_AA)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--video", default="data/videos/2026-09-25 16-12-30.mp4")
    parser.add_argument("--model", default="data/models/qdp_ppe_detection.engine")
    parser.add_argument("--person-model", default="data/models/yolov11-cetection_fp16.engine")
    parser.add_argument("--output", default="output/ppe/2026-09-25_16-12-30_uniform.mp4")
    parser.add_argument("--conf", type=float, default=.25)
    parser.add_argument("--person-conf", type=float, default=.35)
    parser.add_argument("--nms", type=float, default=.45)
    parser.add_argument("--max-frames", type=int, default=0)
    parser.add_argument("--clothing-only", action="store_true",
                        help="Detect and label shirts/pants only; skip person checks and report")
    args = parser.parse_args()

    video_path, output_path = Path(args.video), Path(args.output)
    required_models = [args.model] if args.clothing_only else [args.model, args.person_model]
    if not video_path.is_file() or any(not Path(path).is_file() for path in required_models):
        raise FileNotFoundError("Video or required TensorRT model not found")
    detection_path = output_path.with_suffix(".csv")
    summary_path = output_path.with_name(output_path.stem + "_summary.json")
    report_path = None if args.clothing_only else output_path.with_name(output_path.stem + "_uniform.csv")
    outputs = [output_path, detection_path, summary_path] + ([report_path] if report_path else [])
    if any(path.exists() for path in outputs):
        raise FileExistsError("Refusing to overwrite existing output: " +
                              ", ".join(str(path) for path in outputs if path.exists()))

    ppe_runner = EngineRunner(args.model)
    person_runner = None if args.clothing_only else EngineRunner(args.person_model)
    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"Cannot open video: {video_path}")
    fps = capture.get(cv2.CAP_PROP_FPS)
    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    ok, frame = capture.read()
    if not ok or fps <= 0:
        raise RuntimeError("Cannot read first frame or invalid video FPS")
    height, width = frame.shape[:2]
    output_path.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(str(output_path), cv2.VideoWriter_fourcc(*"mp4v"), fps, (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"Cannot open output video writer: {output_path}")

    tracker = IoUTracker()
    clothing_tracker = IoUTracker(max_age=15, threshold=.2)
    status_counts = {"uniform:ok": 0, "uniform:missing_shirt": 0,
                     "uniform:missing_pants": 0, "uniform:missing_shirt_and_pants": 0}
    total_detections = total_people = processed = 0
    started = time.perf_counter()
    with detection_path.open("w", newline="") as det_file:
        det_writer = csv.writer(det_file)
        det_writer.writerow(["frame_index", "source_timestamp_ms", "class_id", "class_name",
                             "confidence", "x", "y", "width", "height", "track_id"])
        if report_path:
            report_file = report_path.open("w", newline="")
            report_writer = csv.writer(report_file)
            report_writer.writerow(["frame_index", "source_timestamp_ms", "person_index", "track_id",
                                    "confidence", "x", "y", "width", "height", "has_shirt",
                                    "has_pants", "status"])
        else:
            report_file = None
            report_writer = None
        try:
            while ok and (not args.max_frames or processed < args.max_frames):
                tensor, scale, pad_x, pad_y = preprocess(frame)
                if args.clothing_only:
                    people = []
                else:
                    people = [list(person) for person in decode_people(
                        person_runner.infer(tensor), scale, pad_x, pad_y,
                        width, height, args.person_conf, args.nms)]
                    tracker.update(people)
                garments = decode_uniform(ppe_runner.infer(tensor), scale, pad_x, pad_y,
                                          width, height, args.conf, args.nms)
                garments = [list(garment) for garment in garments]
                clothing_tracker.update(garments)
                assignments = associate_uniform(people, garments) if people else {}
                annotated = frame.copy()
                timestamp_ms = processed * 1000.0 / fps

                for person_index, person in enumerate(people):
                    track_id = person[3]
                    shirt_index, pants_index = assignments[person_index][0], assignments[person_index][1]
                    mask = (1 if shirt_index < 0 else 0) | (2 if pants_index < 0 else 0)
                    status = ("uniform:ok" if mask == 0 else "uniform:missing_shirt" if mask == 1 else
                              "uniform:missing_pants" if mask == 2 else "uniform:missing_shirt_and_pants")
                    status_counts[status] += 1
                    total_people += 1
                    label = ("Day du dong phuc" if mask == 0 else "Thieu ao" if mask == 1 else
                             "Thieu quan" if mask == 2 else "Thieu ao + quan")
                    color = (0, 200, 0) if mask == 0 else (0, 0, 255)
                    draw_box(annotated, person[1], f"Nguoi #{track_id} - {label}", color, 2)
                    report_writer.writerow([processed, f"{timestamp_ms:.3f}", person_index + 1, track_id,
                                            f"{person[2]:.6f}", *[f"{v:.2f}" for v in person[1]],
                                            int(shirt_index >= 0), int(pants_index >= 0), status])

                for item_index, garment in enumerate(garments):
                    class_id, box, score = garment[:3]
                    name = "Ao" if class_id == 0 else "Quan"
                    owner = next((person[3] for i, person in enumerate(people)
                                  if assignments[i][class_id] == item_index), None)
                    garment_track_id = garment[3]
                    label = f"{name} #{garment_track_id}" + (f" (nguoi #{owner})" if owner is not None else "")
                    draw_box(annotated, box, label, (0, 0, 255), 2, (255, 255, 255))
                    x1, y1, x2, y2 = box
                    det_writer.writerow([processed, f"{timestamp_ms:.3f}", class_id, name,
                                         f"{score:.6f}", f"{x1:.2f}", f"{y1:.2f}",
                                         f"{x2 - x1:.2f}", f"{y2 - y1:.2f}", garment_track_id])
                    total_detections += 1

                if not args.clothing_only:
                    for person in people:
                        x1, y1, x2, y2 = person[1]
                        det_writer.writerow([processed, f"{timestamp_ms:.3f}", 100, "Nguoi",
                                             f"{person[2]:.6f}", f"{x1:.2f}", f"{y1:.2f}",
                                             f"{x2 - x1:.2f}", f"{y2 - y1:.2f}", person[3]])
                        total_detections += 1
                    summary = (f"DONG PHUC | DAT: {status_counts['uniform:ok']} | "
                               f"THIEU: {sum(status_counts.values()) - status_counts['uniform:ok']}")
                    cv2.putText(annotated, summary, (20, 42), cv2.FONT_HERSHEY_SIMPLEX,
                                max(.6, width / 2200), (255, 255, 255), 2, cv2.LINE_AA)
                writer.write(annotated)
                processed += 1
                if processed % 100 == 0:
                    print(f"Processed {processed} frames", flush=True)
                ok, frame = capture.read()
        finally:
            if report_file:
                report_file.close()

    capture.release()
    writer.release()
    elapsed = time.perf_counter() - started
    summary = {
        "video": str(video_path), "ppe_model": args.model,
        "person_model": None if args.clothing_only else args.person_model,
        "mode": "clothing_only" if args.clothing_only else "person_uniform_assessment",
        "frames_processed": processed, "source_frames": frame_count, "fps_source": fps,
        "detections": total_detections, "person_frame_assessments": total_people,
        "unique_clothing_tracks": clothing_tracker.next_id - 1,
        "uniform_status_counts": None if args.clothing_only else status_counts,
        "processing_fps": processed / elapsed if elapsed else 0,
        "video_output": str(output_path), "detections_csv": str(detection_path),
        "uniform_report_csv": None if args.clothing_only else str(report_path), "nms_threshold": args.nms,
        "garment_confidence_threshold": args.conf, "person_confidence_threshold": args.person_conf,
        "association_note": "Per-frame detection association using box containment and upper/lower body zones; not clothing certification."
    }
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Done: {processed} frames, {total_detections} clothing detections, "
          f"{processed / elapsed:.2f} FPS")
    print(f"Video: {output_path}\nDetections: {detection_path}")
    if report_path:
        print(f"Uniform report: {report_path}")
    print(f"Summary: {summary_path}")


if __name__ == "__main__":
    main()