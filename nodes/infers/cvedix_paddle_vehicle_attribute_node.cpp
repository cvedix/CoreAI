/**
 * @file cvedix_paddle_vehicle_attribute_node.cpp
 * @brief Paddle Inference node for vehicle attribute recognition (PP-LCNet)
 */

#include "cvedix_paddle_vehicle_attribute_node.h"

#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

#include <opencv2/dnn.hpp>

#include "paddle_inference_api.h"

namespace cvedix_nodes {

namespace {

std::string join_path(const std::string& dir, const std::string& file) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (dir.empty()) {
        return file;
    }
    if (dir.back() == '/' || dir.back() == '\\') {
        return dir + file;
    }
    return dir + sep + file;
}

std::string resolve_existing_file(const std::string& dir, const std::vector<std::string>& candidates) {
    namespace fs = std::filesystem;
    for (const auto& name : candidates) {
        const auto path = fs::path(join_path(dir, name));
        if (fs::exists(path)) {
            return path.string();
        }
    }
    return "";
}

void transpose_nchw_to_nhwc(const cv::Mat& src, cv::Mat& dst) {
    if (src.dims != 4) {
        dst = src.clone();
        return;
    }
    int n = src.size[0];
    int c = src.size[1];
    int h = src.size[2];
    int w = src.size[3];

    std::vector<int> new_shape = {n, h, w, c};
    dst = cv::Mat(4, new_shape.data(), src.type());

    const float* src_data = reinterpret_cast<const float*>(src.data);
    float* dst_data = reinterpret_cast<float*>(dst.data);

    for (int ni = 0; ni < n; ++ni) {
        for (int hi = 0; hi < h; ++hi) {
            for (int wi = 0; wi < w; ++wi) {
                for (int ci = 0; ci < c; ++ci) {
                    int src_idx = ni * (c * h * w) + ci * (h * w) + hi * w + wi;
                    int dst_idx = ni * (h * w * c) + hi * (w * c) + wi * c + ci;
                    dst_data[dst_idx] = src_data[src_idx];
                }
            }
        }
    }
}

void apply_std(cv::Mat& blob, const cv::Scalar& std) {
    if (blob.dims != 4) {
        return;
    }

    float std_vals[3] = {static_cast<float>(std[0]), static_cast<float>(std[1]), static_cast<float>(std[2])};
    bool use_std = false;
    for (int i = 0; i < 3; ++i) {
        if (std_vals[i] != 1.0f && std_vals[i] != 0.0f) {
            use_std = true;
            break;
        }
    }
    if (!use_std) {
        return;
    }

    int n = blob.size[0];
    int c = blob.size[1];
    int h = blob.size[2];
    int w = blob.size[3];
    size_t hw = static_cast<size_t>(h) * static_cast<size_t>(w);
    float* data = reinterpret_cast<float*>(blob.data);

    for (int ni = 0; ni < n; ++ni) {
        for (int ci = 0; ci < c && ci < 3; ++ci) {
            float denom = std_vals[ci];
            if (denom == 0.0f || denom == 1.0f) {
                continue;
            }
            float* channel = data + (ni * c + ci) * hw;
            for (size_t i = 0; i < hw; ++i) {
                channel[i] /= denom;
            }
        }
    }
}

float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

struct OutputHead {
    int dim = 0;
    std::vector<float> data;
};

std::string format_secondary_labels(
    const cvedix_objects::cvedix_frame_target& target,
    size_t start_index) {
    if (start_index >= target.secondary_labels.size()) {
        return "none";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    bool first = true;
    const size_t end = std::min(target.secondary_labels.size(), target.secondary_scores.size());
    for (size_t i = start_index; i < end; ++i) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        oss << target.secondary_labels[i] << "(" << target.secondary_scores[i] << ")";
    }
    return first ? "none" : oss.str();
}

}  // namespace

cvedix_paddle_vehicle_attribute_node::cvedix_paddle_vehicle_attribute_node(
    const std::string& node_name,
    const std::string& model_dir,
    const std::string& labels_path,
    bool use_gpu,
    int gpu_id,
    const std::string& run_mode,
    bool use_mkldnn,
    int cpu_threads,
    int input_width,
    int input_height,
    int batch_size,
    float score_threshold,
    const std::vector<int>& attribute_group_sizes,
    std::vector<int> p_class_ids_applied_to,
    int min_width_applied_to,
    int min_height_applied_to,
    int crop_padding,
    bool apply_sigmoid,
    float scale,
    cv::Scalar mean,
    cv::Scalar std,
    bool swap_rb,
    bool swap_chn,
    int trt_min_shape,
    int trt_max_shape,
    int trt_opt_shape,
    bool trt_calib_mode)
    : cvedix_secondary_infer_node(
          node_name,
          "",
          "",
          labels_path,
          input_width,
          input_height,
          batch_size,
          p_class_ids_applied_to,
          min_width_applied_to,
          min_height_applied_to,
          crop_padding,
          scale,
          mean,
          std,
          swap_rb,
          swap_chn),
      model_dir_(model_dir),
      score_threshold_(score_threshold),
      attribute_group_sizes_(attribute_group_sizes),
      apply_sigmoid_(apply_sigmoid),
      use_gpu_(use_gpu),
      gpu_id_(gpu_id),
      run_mode_(run_mode),
      use_mkldnn_(use_mkldnn),
      cpu_threads_(cpu_threads),
      trt_min_shape_(trt_min_shape),
      trt_max_shape_(trt_max_shape),
      trt_opt_shape_(trt_opt_shape),
      trt_calib_mode_(trt_calib_mode) {

#ifdef CVEDIX_WITH_LICENSE
    if (!cvedix_utils::cvedix_license_manager::get_instance().check_license()) {
        throw std::runtime_error("Inference features require a valid license. Please contact support.");
    }
#endif

    if (model_dir_.empty()) {
        throw std::invalid_argument("model_dir is required for vehicle attribute recognition.");
    }

    init_predictor();

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Paddle vehicle attribute node initialized: device=%s, run_mode=%s, batch=%d, labels=%zu",
        node_name.c_str(), use_gpu_ ? "GPU" : "CPU", run_mode_.c_str(), batch_size, labels.size()));

    this->initialized();
}

cvedix_paddle_vehicle_attribute_node::~cvedix_paddle_vehicle_attribute_node() {
    predictor_.reset();
    deinitialized();
}

void cvedix_paddle_vehicle_attribute_node::set_score_threshold(float threshold) {
    score_threshold_ = threshold;
}

void cvedix_paddle_vehicle_attribute_node::set_attribute_group_sizes(const std::vector<int>& sizes) {
    attribute_group_sizes_ = sizes;
}

void cvedix_paddle_vehicle_attribute_node::init_predictor() {
    paddle_infer::Config config;
    const std::string model_file = resolve_existing_file(
        model_dir_, {"model.pdmodel", "inference.pdmodel", "inference.json"});
    const std::string params_file = resolve_existing_file(
        model_dir_, {"model.pdiparams", "inference.pdiparams"});
    if (model_file.empty() || params_file.empty()) {
        throw std::runtime_error("Missing Paddle model files in model_dir.");
    }
    config.SetModel(model_file, params_file);

    if (use_gpu_) {
        config.EnableUseGpu(200, gpu_id_);
        config.SwitchIrOptim(true);

        if (run_mode_ != "paddle") {
            auto precision = paddle_infer::Config::Precision::kFloat32;
            if (run_mode_ == "trt_fp16") {
                precision = paddle_infer::Config::Precision::kHalf;
            } else if (run_mode_ == "trt_int8") {
                precision = paddle_infer::Config::Precision::kInt8;
            }

            config.EnableTensorRtEngine(
                1 << 30,
                batch_size,
                3,
                precision,
                false,
                trt_calib_mode_);

            std::vector<int> min_input_shape = {batch_size, 3, trt_min_shape_, trt_min_shape_};
            std::vector<int> max_input_shape = {batch_size, 3, trt_max_shape_, trt_max_shape_};
            std::vector<int> opt_input_shape = {batch_size, 3, trt_opt_shape_, trt_opt_shape_};
            const std::map<std::string, std::vector<int>> min_shapes = {{"image", min_input_shape}};
            const std::map<std::string, std::vector<int>> max_shapes = {{"image", max_input_shape}};
            const std::map<std::string, std::vector<int>> opt_shapes = {{"image", opt_input_shape}};
            config.SetTRTDynamicShapeInfo(min_shapes, max_shapes, opt_shapes);
        }
    } else {
        config.DisableGpu();
        if (use_mkldnn_) {
            config.EnableMKLDNN();
            config.SetMkldnnCacheCapacity(10);
        }
        config.SetCpuMathLibraryNumThreads(cpu_threads_);
    }

    config.SwitchUseFeedFetchOps(false);
    config.SwitchIrOptim(true);
    config.DisableGlogInfo();
    config.EnableMemoryOptim();

    predictor_ = paddle_infer::CreatePredictor(config);
    if (!predictor_) {
        throw std::runtime_error("Failed to create Paddle Inference predictor for vehicle attributes.");
    }
}

std::string cvedix_paddle_vehicle_attribute_node::get_label(int class_id) const {
    if (class_id >= 0 && class_id < static_cast<int>(labels.size())) {
        return labels[class_id];
    }
    return "class_" + std::to_string(class_id);
}

void cvedix_paddle_vehicle_attribute_node::preprocess_batch(const std::vector<cv::Mat>& mats, cv::Mat& blob) const {
    cv::dnn::blobFromImages(
        mats,
        blob,
        scale,
        cv::Size(input_width, input_height),
        mean,
        swap_rb,
        false,
        CV_32F);

    if (std != cv::Scalar(1)) {
        apply_std(blob, std);
    }

    if (swap_chn) {
        cv::Mat blob_nhwc;
        transpose_nchw_to_nhwc(blob, blob_nhwc);
        blob_nhwc.copyTo(blob);
    }
}

void cvedix_paddle_vehicle_attribute_node::append_attribute_top1(
    const float* scores,
    int dim,
    int label_offset,
    cvedix_objects::cvedix_frame_target& target) const {
    if (dim <= 0) {
        return;
    }

    int best_idx = 0;
    float best_val = scores[0];
    for (int i = 1; i < dim; ++i) {
        if (scores[i] > best_val) {
            best_val = scores[i];
            best_idx = i;
        }
    }

    float score = apply_sigmoid_ ? sigmoid(best_val) : best_val;
    if (score_threshold_ > 0.0f && score < score_threshold_) {
        return;
    }

    int class_id = label_offset + best_idx;
    target.secondary_class_ids.push_back(class_id);
    target.secondary_scores.push_back(score);
    target.secondary_labels.push_back(get_label(class_id));
}

void cvedix_paddle_vehicle_attribute_node::append_attribute_grouped(
    const float* scores,
    int dim,
    int label_offset,
    cvedix_objects::cvedix_frame_target& target) const {
    if (attribute_group_sizes_.empty()) {
        append_attribute_top1(scores, dim, label_offset, target);
        return;
    }

    int offset = 0;
    for (int group_size : attribute_group_sizes_) {
        if (group_size <= 0 || offset >= dim) {
            continue;
        }
        int group_dim = std::min(group_size, dim - offset);
        append_attribute_top1(scores + offset, group_dim, label_offset + offset, target);
        offset += group_size;
    }
}

void cvedix_paddle_vehicle_attribute_node::append_attribute_multilabel(
    const float* scores,
    int dim,
    cvedix_objects::cvedix_frame_target& target) const {
    bool added = false;
    for (int i = 0; i < dim; ++i) {
        float score = apply_sigmoid_ ? sigmoid(scores[i]) : scores[i];
        if (score >= score_threshold_) {
            int class_id = i;
            target.secondary_class_ids.push_back(class_id);
            target.secondary_scores.push_back(score);
            target.secondary_labels.push_back(get_label(class_id));
            added = true;
        }
    }

    if (!added) {
        append_attribute_top1(scores, dim, 0, target);
    }
}

void cvedix_paddle_vehicle_attribute_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (frame_meta_with_batch.empty() || !predictor_) {
        return;
    }

    auto& frame_meta = frame_meta_with_batch[0];
    std::vector<cv::Mat> mats_to_infer;
    std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_target>> targets_to_infer;
    std::vector<size_t> secondary_offsets;

    for (auto& target : frame_meta->targets) {
        if (!need_apply(target->primary_class_id, target->width, target->height)) {
            continue;
        }

        cv::Rect box(target->x, target->y, target->width, target->height);
        if (crop_padding != 0) {
            box = cv::Rect(
                box.x - crop_padding,
                box.y - crop_padding,
                box.width + crop_padding * 2,
                box.height + crop_padding * 2);
            box.x = std::max(box.x, 0);
            box.y = std::max(box.y, 0);
            box.width = std::min(box.width, frame_meta->frame.cols - box.x);
            box.height = std::min(box.height, frame_meta->frame.rows - box.y);
        }

        mats_to_infer.push_back(frame_meta->frame(box));
        targets_to_infer.push_back(target);
        secondary_offsets.push_back(target->secondary_labels.size());
    }

    if (mats_to_infer.empty()) {
        return;
    }

    auto start_time = std::chrono::system_clock::now();
    int total_preprocess = 0;
    int total_infer = 0;
    int total_postprocess = 0;

    const auto input_names = predictor_->GetInputNames();
    const auto output_names = predictor_->GetOutputNames();

    for (size_t offset = 0; offset < mats_to_infer.size(); offset += batch_size) {
        size_t cur_batch = std::min(static_cast<size_t>(batch_size), mats_to_infer.size() - offset);
        std::vector<cv::Mat> batch_mats(mats_to_infer.begin() + offset, mats_to_infer.begin() + offset + cur_batch);

        cv::Mat blob;
        auto preprocess_start = std::chrono::system_clock::now();
        preprocess_batch(batch_mats, blob);
        auto preprocess_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - preprocess_start);
        total_preprocess += static_cast<int>(preprocess_time.count());

        auto input_handle = predictor_->GetInputHandle(input_names[0]);
        if (!swap_chn) {
            input_handle->Reshape({static_cast<int>(cur_batch), 3, input_height, input_width});
        } else {
            input_handle->Reshape({static_cast<int>(cur_batch), input_height, input_width, 3});
        }
        input_handle->CopyFromCpu(reinterpret_cast<float*>(blob.data));

        auto infer_start = std::chrono::system_clock::now();
        predictor_->Run();
        auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - infer_start);
        total_infer += static_cast<int>(infer_time.count());

        std::vector<OutputHead> heads;
        int total_dim = 0;
        for (const auto& name : output_names) {
            auto output_handle = predictor_->GetOutputHandle(name);
            auto shape = output_handle->shape();
            int64_t total_count = 1;
            for (auto s : shape) {
                total_count *= s;
            }

            int dim = 0;
            if (shape.size() >= 2) {
                dim = static_cast<int>(total_count / cur_batch);
            } else if (shape.size() == 1 && cur_batch > 0) {
                dim = static_cast<int>(total_count / cur_batch);
                if (dim * static_cast<int64_t>(cur_batch) != total_count) {
                    dim = static_cast<int>(total_count);
                }
            } else {
                dim = static_cast<int>(total_count);
            }

            if (dim <= 0) {
                continue;
            }

            OutputHead head;
            head.dim = dim;
            head.data.resize(static_cast<size_t>(cur_batch) * static_cast<size_t>(dim));
            output_handle->CopyToCpu(head.data.data());
            heads.push_back(std::move(head));
            total_dim += dim;
        }

        auto post_start = std::chrono::system_clock::now();
        bool use_label_offsets = (!labels.empty() && labels.size() == static_cast<size_t>(total_dim));
        std::vector<int> head_offsets;
        head_offsets.reserve(heads.size());
        int running_offset = 0;
        for (const auto& head : heads) {
            head_offsets.push_back(running_offset);
            running_offset += head.dim;
        }

        for (size_t i = 0; i < cur_batch; ++i) {
            auto& target = *targets_to_infer[offset + i];

            if (heads.empty()) {
                continue;
            }

            if (heads.size() > 1) {
                for (size_t h = 0; h < heads.size(); ++h) {
                    const auto& head = heads[h];
                    int label_offset = use_label_offsets ? head_offsets[h] : 0;
                    const float* scores = head.data.data() + i * head.dim;
                    append_attribute_top1(scores, head.dim, label_offset, target);
                }
            } else if (!attribute_group_sizes_.empty()) {
                const auto& head = heads[0];
                const float* scores = head.data.data() + i * head.dim;
                int label_offset = use_label_offsets ? head_offsets[0] : 0;
                append_attribute_grouped(scores, head.dim, label_offset, target);
            } else {
                const auto& head = heads[0];
                const float* scores = head.data.data() + i * head.dim;
                append_attribute_multilabel(scores, head.dim, target);
            }

            const size_t base_index = secondary_offsets[offset + i];
            const std::string attr_text = format_secondary_labels(target, base_index);
            if (attr_text != "none") {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] attr frame=%d ch=%d track=%d bbox=(%d,%d,%d,%d) %s",
                    node_name.c_str(),
                    target.frame_index,
                    target.channel_index,
                    target.track_id,
                    target.x,
                    target.y,
                    target.width,
                    target.height,
                    attr_text.c_str()));
            }
        }

        auto post_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - post_start);
        total_postprocess += static_cast<int>(post_time.count());
    }

    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);

    infer_combinations_time_cost(
        static_cast<int>(mats_to_infer.size()),
        0,
        total_preprocess,
        total_infer,
        total_postprocess);

    (void)total_time;
}

void cvedix_paddle_vehicle_attribute_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    (void)raw_outputs;
    (void)frame_meta_with_batch;
}

}  // namespace cvedix_nodes
