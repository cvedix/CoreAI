/**
 * @file cvedix_paddle_detector_node.cpp
 * @brief PaddleDetection object detector node (CPU/GPU)
 */

#include "cvedix_paddle_detector_node.h"

#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"


#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace cvedix_nodes {

static std::vector<std::string> load_labels_from_file(const std::string& labels_path) {
    std::vector<std::string> labels;
    std::ifstream file(labels_path);
    if (!file.is_open()) {
        return labels;
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            labels.push_back(line);
        }
    }

    return labels;
}

cvedix_paddle_detector_node::cvedix_paddle_detector_node(
    const std::string& node_name,
    const std::string& model_dir,
    const std::string& labels_path,
    bool use_gpu,
    int gpu_id,
    const std::string& run_mode,
    bool use_mkldnn,
    int cpu_threads,
    int batch_size,
    float conf_threshold,
    int class_id_offset,
    int trt_min_shape,
    int trt_max_shape,
    int trt_opt_shape,
    bool trt_calib_mode)
    : cvedix_primary_infer_node(node_name, "", "", "", 0, 0, batch_size, class_id_offset),
      conf_threshold(conf_threshold) {


    try {
        const std::string device = use_gpu ? "GPU" : "CPU";
        detector_ = std::make_unique<PaddleDetection::ObjectDetector>(
            model_dir,
            device,
            use_mkldnn,
            cpu_threads,
            run_mode,
            batch_size,
            gpu_id,
            trt_min_shape,
            trt_max_shape,
            trt_opt_shape,
            trt_calib_mode);

        if (!labels_path.empty()) {
            labels = load_labels_from_file(labels_path);
        } else {
            labels = detector_->GetLabelList();
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Paddle detector initialized: device=%s, run_mode=%s, batch=%d, labels=%zu",
            node_name.c_str(), device.c_str(), run_mode.c_str(), batch_size, labels.size()));

        this->initialized();
    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to initialize Paddle detector: %s",
            node_name.c_str(), e.what()));
        throw;
    }
}

cvedix_paddle_detector_node::~cvedix_paddle_detector_node() {
    detector_.reset();
    deinitialized();
}

void cvedix_paddle_detector_node::set_conf_threshold(float thresh) {
    conf_threshold = thresh;
}

std::string cvedix_paddle_detector_node::get_label(int class_id) const {
    int idx = class_id - class_id_offset;
    if (idx >= 0 && idx < static_cast<int>(labels.size())) {
        return labels[idx];
    }
    return "class_" + std::to_string(class_id);
}

void cvedix_paddle_detector_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (frame_meta_with_batch.empty() || !detector_) {
        return;
    }

    auto prepare_start = std::chrono::system_clock::now();

    std::vector<cv::Mat> frames;
    frames.reserve(frame_meta_with_batch.size());
    for (const auto& meta : frame_meta_with_batch) {
        frames.push_back(meta->frame);
    }

    auto prepare_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - prepare_start);

    std::vector<PaddleDetection::ObjectResult> results;
    std::vector<int> bbox_num;
    std::vector<double> times;

    detector_->Predict(frames, conf_threshold, 0, 1, &results, &bbox_num, &times);

    size_t result_offset = 0;
    for (size_t i = 0; i < frame_meta_with_batch.size(); ++i) {
        auto& frame_meta = frame_meta_with_batch[i];
        const auto& frame = frame_meta->frame;

        int det_count = (i < bbox_num.size()) ? bbox_num[i] : 0;
        for (int j = 0; j < det_count && (result_offset + j) < results.size(); ++j) {
            const auto& det = results[result_offset + j];
            if (det.confidence < conf_threshold) {
                continue;
            }

            int xmin = 0;
            int ymin = 0;
            int xmax = 0;
            int ymax = 0;

            if (det.rect.size() >= 8) {
                xmin = det.rect[0];
                xmax = det.rect[0];
                ymin = det.rect[1];
                ymax = det.rect[1];
                for (size_t k = 0; k + 1 < 8; k += 2) {
                    xmin = std::min(xmin, det.rect[k]);
                    xmax = std::max(xmax, det.rect[k]);
                    ymin = std::min(ymin, det.rect[k + 1]);
                    ymax = std::max(ymax, det.rect[k + 1]);
                }
            } else if (det.rect.size() >= 4) {
                xmin = det.rect[0];
                ymin = det.rect[1];
                xmax = det.rect[2];
                ymax = det.rect[3];
            } else {
                continue;
            }

            xmin = std::max(0, xmin);
            ymin = std::max(0, ymin);
            xmax = std::min(xmax, frame.cols - 1);
            ymax = std::min(ymax, frame.rows - 1);

            int width = xmax - xmin;
            int height = ymax - ymin;
            if (width <= 0 || height <= 0) {
                continue;
            }

            int cid = det.class_id + class_id_offset;
            if (!allowed_class_ids.empty() && allowed_class_ids.find(cid) == allowed_class_ids.end()) {
                continue;
            }

            auto label = get_label(cid);
            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                xmin, ymin, width, height,
                cid,
                det.confidence,
                frame_meta->frame_index,
                frame_meta->channel_index,
                label);

            if (!det.mask.empty() && det.mask.size() == static_cast<size_t>(frame.rows * frame.cols)) {
                cv::Mat mask(frame.rows, frame.cols, CV_32S);
                std::memcpy(mask.data, det.mask.data(), det.mask.size() * sizeof(int));
                target->mask = mask;
            }

            frame_meta->targets.push_back(target);
        }

        result_offset += det_count;
    }

    int preprocess_ms = times.size() > 0 ? static_cast<int>(times[0]) : 0;
    int infer_ms = times.size() > 1 ? static_cast<int>(times[1]) : 0;
    int postprocess_ms = times.size() > 2 ? static_cast<int>(times[2]) : 0;

    cvedix_infer_node::infer_combinations_time_cost(
        frames.size(),
        prepare_time.count(),
        preprocess_ms,
        infer_ms,
        postprocess_ms);
}

void cvedix_paddle_detector_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    (void)raw_outputs;
    (void)frame_meta_with_batch;
}

}  // namespace cvedix_nodes
