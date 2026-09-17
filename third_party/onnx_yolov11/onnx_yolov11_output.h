#pragma once

#include <algorithm>
#include <opencv2/core.hpp>
#include <stdexcept>

namespace onnx_yolov11 {

// Raw YOLO11 detection heads have 4 box coordinates followed by class scores.
// Support both [1, 4+nc, anchors] and [1, anchors, 4+nc], including custom nc.
// End-to-end/NMS exports are deliberately not supported by this decoder.
inline cv::Mat normalize_output(const cv::Mat& output) {
    if (output.type() != CV_32F || output.empty() ||
        (output.dims != 2 && output.dims != 3) ||
        (output.dims == 3 && output.size[0] != 1)) {
        throw std::runtime_error("YOLO11 requires a float raw detection head with batch size 1");
    }
    const int rows = output.dims == 3 ? output.size[1] : output.rows;
    const int cols = output.dims == 3 ? output.size[2] : output.cols;
    if (std::min(rows, cols) <= 4 || rows == cols) {
        throw std::runtime_error("Invalid YOLO11 raw detection head shape");
    }
    cv::Mat flat = output.reshape(1, rows);
    if (rows < cols) return flat.t();
    return flat;
}

} // namespace onnx_yolov11
