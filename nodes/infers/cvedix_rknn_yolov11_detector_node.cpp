#include "cvedix_rknn_yolov11_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {

    cvedix_rknn_yolov11_detector_node::cvedix_rknn_yolov11_detector_node(
        std::string node_name,
        std::string model_path,
        float score_threshold,
        float nms_threshold,
        int input_width,
        int input_height,
        int num_classes,
        std::string labels_path,
        int class_id_offset)
        : cvedix_primary_infer_node(node_name,
                                    model_path,
                                    "",
                                    labels_path,
                                    input_width,
                                    input_height,
                                    1,
                                    class_id_offset),
          score_threshold(score_threshold),
          nms_threshold(nms_threshold),
          input_width(input_width),
          input_height(input_height),
          num_classes(num_classes) {

        rknn_helper = std::make_shared<cvedix_utils::cvedix_rknn_helper>();
        int ret = rknn_helper->init(model_path);
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to initialize RKNN model: %s",
                node_name.c_str(),
                model_path.c_str()));
            throw std::runtime_error("RKNN initialization failed");
        }

#ifdef CVEDIX_WITH_RGA
        rga_helper = std::make_shared<cvedix_utils::cvedix_rga_helper>();
        use_rga = rga_helper->init();
        if (use_rga) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] RGA acceleration enabled", node_name.c_str()));
        } else {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] RGA not available, using OpenCV", node_name.c_str()));
        }
#else
        rga_helper = nullptr;
        use_rga = false;
        CVEDIX_INFO(cvedix_utils::string_format("[%s] RGA not compiled, using OpenCV", node_name.c_str()));
#endif

        sync_model_input_shape();

        this->initialized();
    }

    cvedix_rknn_yolov11_detector_node::~cvedix_rknn_yolov11_detector_node() {
        deinitialized();
    }

    void cvedix_rknn_yolov11_detector_node::sync_model_input_shape() {
        if (!rknn_helper || !rknn_helper->is_initialized()) {
            return;
        }

        auto attr = rknn_helper->get_input_attr(0);
        int inferred_width = input_width;
        int inferred_height = input_height;

        if (attr.n_dims >= 4) {
            switch (attr.fmt) {
                case RKNN_TENSOR_NCHW:
                    inferred_height = attr.dims[2];
                    inferred_width = attr.dims[3];
                    break;
                case RKNN_TENSOR_NHWC:
                    inferred_height = attr.dims[1];
                    inferred_width = attr.dims[2];
                    break;
                default:
                    inferred_height = attr.dims[attr.n_dims - 2];
                    inferred_width = attr.dims[attr.n_dims - 1];
                    break;
            }
        }

        if (inferred_width <= 0 || inferred_height <= 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Unable to infer RKNN input shape, using configured %dx%d",
                                                    node_name.c_str(),
                                                    input_width,
                                                    input_height));
            return;
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] RKNN input attr dims: n_dims=%d fmt=%d -> height=%d width=%d",
                                                 node_name.c_str(),
                                                 attr.n_dims,
                                                 attr.fmt,
                                                 inferred_height,
                                                 inferred_width));

        input_width = inferred_width;
        input_height = inferred_height;

        CVEDIX_INFO(cvedix_utils::string_format("[%s] RKNN input shape synchronised to %dx%d",
                                                node_name.c_str(),
                                                input_width,
                                                input_height));
    }

    void cvedix_rknn_yolov11_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer,
                                                       cv::Mat& blob_to_infer) {
        if (mats_to_infer.empty()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] No input images", node_name.c_str()));
            return;
        }

        cv::Mat input_image = mats_to_infer[0];
        if (input_image.empty() || input_image.cols <= 0 || input_image.rows <= 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Input frame is empty", node_name.c_str()));
            return;
        }

        int effective_width = input_width;
        int effective_height = input_height;
        if (effective_width <= 0) {
            effective_width = input_image.cols;
        }
        if (effective_height <= 0) {
            effective_height = input_image.rows;
        }

        if (effective_width <= 0 || effective_height <= 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Invalid target size (%d, %d)",
                                                     node_name.c_str(),
                                                     effective_width,
                                                     effective_height));
            return;
        }

        const float width_scale = static_cast<float>(effective_width) / static_cast<float>(input_image.cols);
        const float height_scale = static_cast<float>(effective_height) / static_cast<float>(input_image.rows);
        const float scale = std::min(width_scale, height_scale);

        int resized_width = static_cast<int>(std::round(input_image.cols * scale));
        int resized_height = static_cast<int>(std::round(input_image.rows * scale));

        resized_width = std::max(1, std::min(resized_width, effective_width));
        resized_height = std::max(1, std::min(resized_height, effective_height));

        const int pad_w = std::max(0, effective_width - resized_width);
        const int pad_h = std::max(0, effective_height - resized_height);
        const int pad_left = pad_w / 2;
        const int pad_right = pad_w - pad_left;
        const int pad_top = pad_h / 2;
        const int pad_bottom = pad_h - pad_top;

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Preprocess plan: src=%dx%d dst=%dx%d scale=%.4f pad(L=%d,R=%d,T=%d,B=%d)",
                                                 node_name.c_str(),
                                                 input_image.cols,
                                                 input_image.rows,
                                                 effective_width,
                                                 effective_height,
                                                 scale,
                                                 pad_left,
                                                 pad_right,
                                                 pad_top,
                                                 pad_bottom));

        cv::Mat resized;

#ifdef CVEDIX_WITH_RGA
        if (use_rga && rga_helper && rga_helper->is_available()) {
            rga_helper->resize(input_image, resized, cv::Size(resized_width, resized_height));
            cv::Mat rgb_image;
            rga_helper->cvt_color(resized, rgb_image, cv::COLOR_BGR2RGB);
            resized = rgb_image;
        } else {
            cv::resize(input_image, resized, cv::Size(resized_width, resized_height));
            cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        }
#else
        cv::resize(input_image, resized, cv::Size(resized_width, resized_height));
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
#endif

        if (resized.empty() || resized.cols <= 0 || resized.rows <= 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Preprocess failed, processed frame is empty", node_name.c_str()));
            return;
        }

        cv::Mat processed(effective_height, effective_width, resized.type(), cv::Scalar(114, 114, 114));
        cv::Mat roi = processed(cv::Rect(pad_left, pad_top, resized.cols, resized.rows));
        resized.copyTo(roi);

        letterbox_params params;
        params.scale = scale;
        params.pad_left = pad_left;
        params.pad_right = pad_right;
        params.pad_top = pad_top;
        params.pad_bottom = pad_bottom;
        params.padded_width = effective_width;
        params.padded_height = effective_height;
        params.original_size = input_image.size();

        input_width = effective_width;
        input_height = effective_height;

        {
            std::lock_guard<std::mutex> guard(letterbox_mutex);
            pending_letterbox_params.push_back(params);
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Processed frame size: %dx%d (resized=%dx%d)",
                                                 node_name.c_str(),
                                                 processed.cols,
                                                 processed.rows,
                                                 resized.cols,
                                                 resized.rows));

        bool use_float = true;
        bool use_nchw = true;
        if (rknn_helper && rknn_helper->is_initialized()) {
            auto attr = rknn_helper->get_input_attr(0);
            if (attr.type == RKNN_TENSOR_UINT8 || attr.type == RKNN_TENSOR_INT8) {
                use_float = false;
            }
            if (attr.fmt == RKNN_TENSOR_NHWC) {
                use_nchw = false;
            }
        }

        if (use_float) {
            processed.convertTo(processed, CV_32F, 1.0f / 255.0f);
        }

        const int channel_size = effective_height * effective_width;
        if (channel_size <= 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Invalid channel size %d (input %dx%d)",
                                                     node_name.c_str(),
                                                     channel_size,
                                                     effective_width,
                                                     effective_height));
            return;
        }

        const int total_pixels = channel_size;
        const int num_channels = 3;

        if (use_nchw) {
            std::vector<cv::Mat> channels;
            cv::split(processed, channels);

            if (channels.size() != 3) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Preprocess failed, expected 3 channels but got %zu",
                                                         node_name.c_str(),
                                                         channels.size()));
                return;
            }

            if (use_float) {
                blob_to_infer = cv::Mat(1, total_pixels * num_channels, CV_32F);
                float* blob_data = blob_to_infer.ptr<float>();
                for (int c = 0; c < 3; ++c) {
                    memcpy(blob_data + c * channel_size, channels[c].data, channel_size * sizeof(float));
                }
            } else {
                blob_to_infer = cv::Mat(1, total_pixels * num_channels, CV_8U);
                uint8_t* blob_data = blob_to_infer.ptr<uint8_t>();
                for (int c = 0; c < 3; ++c) {
                    memcpy(blob_data + c * channel_size, channels[c].data, channel_size);
                }
            }
        } else {
            if (processed.isContinuous()) {
                blob_to_infer = processed.reshape(1, 1).clone();
            } else {
                blob_to_infer = processed.clone().reshape(1, 1);
            }
        }
    }

    void cvedix_rknn_yolov11_detector_node::infer(const cv::Mat& blob_to_infer,
                                                  std::vector<cv::Mat>& raw_outputs) {
        if (!rknn_helper || !rknn_helper->is_initialized()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] RKNN helper not initialized", node_name.c_str()));
            return;
        }

        auto attr = rknn_helper->get_input_attr(0);
        cv::Mat input_mat;

        if (attr.fmt == RKNN_TENSOR_NHWC) {
            int reshape_dims[] = {1, input_height, input_width, 3};
            input_mat = blob_to_infer.reshape(1, 4, reshape_dims);
        } else {
            int reshape_dims[] = {1, 3, input_height, input_width};
            input_mat = blob_to_infer.reshape(1, 4, reshape_dims);
        }

        int ret = rknn_helper->set_input(0, input_mat);
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to set RKNN input", node_name.c_str()));
            return;
        }

        ret = rknn_helper->run();
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] RKNN inference failed", node_name.c_str()));
            return;
        }

        int tensor_count = rknn_helper->get_output_num();
        raw_outputs.clear();
        raw_outputs.reserve(tensor_count);

        for (int i = 0; i < tensor_count; ++i) {
            auto output_attr = rknn_helper->get_output_attr(i);
            std::vector<int> shape;
            for (int d = 0; d < output_attr.n_dims; ++d) {
                shape.push_back(output_attr.dims[d]);
            }
            cv::Mat output_mat = rknn_helper->get_output_mat(i, shape);
            if (!output_mat.empty()) {
                raw_outputs.push_back(output_mat);
            }
        }
    }

    void cvedix_rknn_yolov11_detector_node::parse_yolov11_output(const float* output_data,
                                                                 int output_size,
                                                                 const rknn_tensor_attr& output_attr,
                                                                 int parsed_num_classes,
                                                                 const letterbox_params& lb_params,
                                                                 const cv::Size& frame_size,
                                                                 std::vector<cv::Rect>& boxes,
                                                                 std::vector<float>& scores,
                                                                 std::vector<int>& class_ids) {
        boxes.clear();
        scores.clear();
        class_ids.clear();

        if (!output_data || output_size <= 0) {
            return;
        }

        const int effective_num_classes = parsed_num_classes > 0 ? parsed_num_classes : 1;
        const int stride_candidate_with_obj = effective_num_classes + 5;   // x,y,w,h,obj + cls
        const int stride_candidate_without_obj = effective_num_classes + 4; // x,y,w,h + cls

        int stride = 0;
        bool has_objectness = false;

        if (stride_candidate_with_obj > 0 && output_size % stride_candidate_with_obj == 0) {
            stride = stride_candidate_with_obj;
            has_objectness = true;
        } else if (stride_candidate_without_obj > 0 && output_size % stride_candidate_without_obj == 0) {
            stride = stride_candidate_without_obj;
            has_objectness = false;
        } else if (output_attr.n_dims >= 2) {
            for (int d = output_attr.n_dims - 1; d >= 0; --d) {
                int dim = static_cast<int>(output_attr.dims[d]);
                if (dim <= 0) {
                    continue;
                }
                if (dim == stride_candidate_with_obj) {
                    stride = dim;
                    has_objectness = true;
                    break;
                }
                if (dim == stride_candidate_without_obj) {
                    stride = dim;
                    has_objectness = false;
                    break;
                }
            }
        }

        if (stride <= 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Unable to deduce YOLOv11 stride from output (size=%d, classes=%d, n_dims=%d)",
                                                    node_name.c_str(),
                                                    output_size,
                                                    effective_num_classes,
                                                    output_attr.n_dims));
            return;
        }

        if (output_size % stride != 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] YOLOv11 output size %d is not divisible by stride %d",
                                                    node_name.c_str(),
                                                    output_size,
                                                    stride));
        }

        const int num_boxes = output_size / stride;
        if (num_boxes <= 0) {
            return;
        }

        const float padded_w = lb_params.padded_width > 0 ? static_cast<float>(lb_params.padded_width) : static_cast<float>(input_width);
        const float padded_h = lb_params.padded_height > 0 ? static_cast<float>(lb_params.padded_height) : static_cast<float>(input_height);
        const float inferred_scale = lb_params.scale > 1e-6f
                                         ? lb_params.scale
                                         : std::min(
                                               padded_w > 0 ? padded_w / std::max(1, lb_params.original_size.width) : 1.0f,
                                               padded_h > 0 ? padded_h / std::max(1, lb_params.original_size.height) : 1.0f);
        const float pad_left_f = static_cast<float>(lb_params.pad_left);
        const float pad_top_f = static_cast<float>(lb_params.pad_top);

        const float frame_width_f = static_cast<float>(frame_size.width);
        const float frame_height_f = static_cast<float>(frame_size.height);

        auto sigmoid = [](float x) -> float {
            return 1.0f / (1.0f + std::exp(-x));
        };

        for (int i = 0; i < num_boxes; ++i) {
            const float* row = output_data + i * stride;
            if (!row) {
                continue;
            }

            float x_center = row[0];
            float y_center = row[1];
            float width = row[2];
            float height = row[3];

            const float max_coord = std::max(std::max(std::fabs(x_center), std::fabs(y_center)),
                                             std::max(std::fabs(width), std::fabs(height)));

            if (max_coord <= 1.5f) {
                x_center *= padded_w;
                y_center *= padded_h;
                width *= padded_w;
                height *= padded_h;
            }

            width = std::max(width, 0.0f);
            height = std::max(height, 0.0f);

            float objectness = has_objectness ? sigmoid(row[4]) : 1.0f;
            objectness = std::clamp(objectness, 0.0f, 1.0f);

            int class_start_index = has_objectness ? 5 : 4;
            class_start_index = std::min(class_start_index, stride);
            const int tail_values = stride - class_start_index;

            int best_class = 0;
            float best_score = objectness;

            if (effective_num_classes > 0 && tail_values >= effective_num_classes) {
                best_score = -1.0f;
                for (int c = 0; c < effective_num_classes; ++c) {
                    const float class_conf = sigmoid(row[class_start_index + c]);
                    const float final_score = has_objectness ? objectness * class_conf : class_conf;
                    if (final_score > best_score) {
                        best_score = final_score;
                        best_class = c;
                    }
                }
            } else if (effective_num_classes == 1 && tail_values >= 1) {
                const float class_prob = sigmoid(row[class_start_index]);
                best_score = has_objectness ? objectness * class_prob : class_prob;
            } else {
                best_score = objectness;
            }

            if (best_score < score_threshold) {
                continue;
            }

            float x1 = x_center - width * 0.5f;
            float y1 = y_center - height * 0.5f;
            float x2 = x_center + width * 0.5f;
            float y2 = y_center + height * 0.5f;

            x1 = std::max(0.0f, std::min(x1, padded_w));
            y1 = std::max(0.0f, std::min(y1, padded_h));
            x2 = std::max(0.0f, std::min(x2, padded_w));
            y2 = std::max(0.0f, std::min(y2, padded_h));

            x1 = x1 - pad_left_f;
            y1 = y1 - pad_top_f;
            x2 = x2 - pad_left_f;
            y2 = y2 - pad_top_f;

            if (inferred_scale > 1e-6f) {
                x1 = x1 / inferred_scale;
                y1 = y1 / inferred_scale;
                x2 = x2 / inferred_scale;
                y2 = y2 / inferred_scale;
            }

            int x = static_cast<int>(std::round(x1));
            int y = static_cast<int>(std::round(y1));
            int w = static_cast<int>(std::round(x2 - x1));
            int h = static_cast<int>(std::round(y2 - y1));

            x = std::max(0, std::min(x, static_cast<int>(frame_width_f) - 1));
            y = std::max(0, std::min(y, static_cast<int>(frame_height_f) - 1));
            w = std::max(1, std::min(w, static_cast<int>(frame_width_f) - x));
            h = std::max(1, std::min(h, static_cast<int>(frame_height_f) - y));

            boxes.emplace_back(x, y, w, h);
            scores.emplace_back(best_score);
            class_ids.emplace_back(best_class);
        }
    }

    void cvedix_rknn_yolov11_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Empty outputs or frame meta", node_name.c_str()));
            return;
        }

        auto& frame_meta = frame_meta_with_batch[0];
        cv::Mat& frame = frame_meta->frame;

        const cv::Mat& output = raw_outputs[0];
        const float* output_data = output.ptr<float>();
        const int output_size = static_cast<int>(output.total());
        auto output_attr = rknn_helper->get_output_attr(0);

        static bool logged_output_info = false;
        if (!logged_output_info) {
            std::string dims_str = "[";
            for (int d = 0; d < output_attr.n_dims; ++d) {
                if (d > 0) dims_str += ", ";
                dims_str += std::to_string(output_attr.dims[d]);
            }
            dims_str += "]";
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] YOLOv11 output tensor: n_dims=%d dims=%s fmt=%d type=%d size=%d total_elements=%d",
                node_name.c_str(), output_attr.n_dims, dims_str.c_str(),
                output_attr.fmt, output_attr.type, output_attr.size, output_size));
            logged_output_info = true;
        }

        int effective_num_classes = num_classes;
        if (!labels.empty()) {
            effective_num_classes = static_cast<int>(labels.size());
        } else if (effective_num_classes <= 0) {
            effective_num_classes = 1;
        }

        letterbox_params lb_params;
        {
            std::lock_guard<std::mutex> guard(letterbox_mutex);
            if (!pending_letterbox_params.empty()) {
                lb_params = pending_letterbox_params.front();
                pending_letterbox_params.pop_front();
            }
        }
        if (lb_params.original_size.width <= 0 || lb_params.original_size.height <= 0) {
            lb_params.original_size = frame.size();
        }
        if (lb_params.padded_width <= 0) {
            lb_params.padded_width = input_width;
        }
        if (lb_params.padded_height <= 0) {
            lb_params.padded_height = input_height;
        }

        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        std::vector<int> class_ids;

        parse_yolov11_output(output_data,
                             output_size,
                             output_attr,
                             effective_num_classes,
                             lb_params,
                             frame.size(),
                             boxes,
                             scores,
                             class_ids);

        if (boxes.empty()) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] No detections above threshold", node_name.c_str()));
            return;
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, scores, score_threshold, nms_threshold, indices);

        for (int idx : indices) {
            const cv::Rect& box = boxes[idx];
            const float score = scores[idx];
            const int cls = class_ids[idx];
            const int global_class_id = cls + class_id_offset;

            std::string label;
            if (!labels.empty() && cls >= 0 && cls < static_cast<int>(labels.size())) {
                label = labels[cls];
            }

            if (!frame.empty()) {
                cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                std::ostringstream score_stream;
                score_stream << std::fixed << std::setprecision(2) << score;
                std::string caption = label.empty()
                                          ? score_stream.str()
                                          : (label + " " + score_stream.str());

                const int base_line = 0;
                cv::Size text_size = cv::getTextSize(caption, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, nullptr);
                cv::Point text_origin(box.x, std::max(0, box.y - 4));
                if (text_origin.y - text_size.height < 0) {
                    text_origin.y = std::min(frame.rows - 1, box.y + text_size.height + 4);
                }
                cv::putText(frame, caption, text_origin, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            }

            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                box.x,
                box.y,
                box.width,
                box.height,
                global_class_id,
                score,
                frame_meta->frame_index,
                frame_meta->channel_index,
                label);

            frame_meta->targets.push_back(target);

            if (effective_num_classes == 1) {
                auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                    box.x,
                    box.y,
                    box.width,
                    box.height,
                    score);
                frame_meta->face_targets.push_back(face_target);
            }

            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] target box=(x=%d, y=%d, w=%d, h=%d) score=%.4f class=%d",
                node_name.c_str(),
                box.x,
                box.y,
                box.width,
                box.height,
                score,
                cls));
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Detected %zu objects",
                                                node_name.c_str(),
                                                frame_meta->targets.size()));
    }

} // namespace cvedix_nodes


