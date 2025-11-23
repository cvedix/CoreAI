#ifdef CVEDIX_WITH_RKNN

#include "cvedix_rknn_yolov8_detector_node.h"
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

    cvedix_rknn_yolov8_detector_node::cvedix_rknn_yolov8_detector_node(
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
#endif

        sync_model_input_shape();

        this->initialized();
    }

    cvedix_rknn_yolov8_detector_node::~cvedix_rknn_yolov8_detector_node() {
        deinitialized();
    }

    void cvedix_rknn_yolov8_detector_node::sync_model_input_shape() {
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

    void cvedix_rknn_yolov8_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer,
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
        if (use_rga && rga_helper->is_available()) {
            rga_helper->resize(input_image, resized, cv::Size(resized_width, resized_height));
            cv::Mat rgb_image;
            rga_helper->cvt_color(resized, rgb_image, cv::COLOR_BGR2RGB);
            resized = rgb_image;
        } else
#endif
        {
            cv::resize(input_image, resized, cv::Size(resized_width, resized_height));
            cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        }

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

        // Determine input requirements
        bool use_float = true;
        bool use_nchw = true;
        if (rknn_helper && rknn_helper->is_initialized()) {
            auto attr = rknn_helper->get_input_attr(0);
            // Don't use float for UINT8/INT8 inputs to avoid zeroing out data
            if (attr.type == RKNN_TENSOR_UINT8 || attr.type == RKNN_TENSOR_INT8) {
                use_float = false;
            }
            // Check for NHWC (interleaved) format
            if (attr.fmt == RKNN_TENSOR_NHWC) {
                use_nchw = false;
            }
        }

        if (use_float) {
            processed.convertTo(processed, CV_32F, 1.0f / 255.0f);
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Channel size: %d (width=%d height=%d) float=%d nchw=%d",
                                                 node_name.c_str(),
                                                 effective_width * effective_height,
                                                 effective_width,
                                                 effective_height,
                                                 use_float,
                                                 use_nchw));

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
            // NHWC - interleaved
            if (use_float) {
                if (processed.isContinuous()) {
                    blob_to_infer = processed.reshape(1, 1).clone();
                } else {
                    blob_to_infer = processed.clone().reshape(1, 1);
                }
            } else {
                if (processed.isContinuous()) {
                    blob_to_infer = processed.reshape(1, 1).clone();
                } else {
                    blob_to_infer = processed.clone().reshape(1, 1);
                }
            }
        }
    }

    void cvedix_rknn_yolov8_detector_node::infer(const cv::Mat& blob_to_infer,
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

    void cvedix_rknn_yolov8_detector_node::parse_yolov8_output(const float* output_data,
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

        if (output_data == nullptr || output_size <= 0) {
            return;
        }

        const int effective_num_classes = parsed_num_classes > 0 ? parsed_num_classes : 1;
        const int stride_candidate_with_obj = effective_num_classes + 5;
        const int stride_candidate_without_obj = effective_num_classes + 4;
        const int stride_candidate_with_obj_and_class_index = effective_num_classes + 6;

        int stride = 0;
        bool has_objectness = false;
        bool has_explicit_class_index = false;

        if (stride_candidate_with_obj > 0 && output_size % stride_candidate_with_obj == 0) {
            stride = stride_candidate_with_obj;
            has_objectness = true;
        } else if (stride_candidate_without_obj > 0 && output_size % stride_candidate_without_obj == 0) {
            stride = stride_candidate_without_obj;
            has_objectness = false;
        } else if (effective_num_classes == 1 &&
                   stride_candidate_with_obj_and_class_index > 0 &&
                   output_size % stride_candidate_with_obj_and_class_index == 0) {
            stride = stride_candidate_with_obj_and_class_index;
            has_objectness = true;
            has_explicit_class_index = true;
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
                if (effective_num_classes == 1 && dim == stride_candidate_with_obj_and_class_index) {
                    stride = dim;
                    has_objectness = true;
                    has_explicit_class_index = true;
                    break;
                }
                if (stride == 0 && dim > 4 && dim <= 128) {
                    stride = dim;
                }
            }
        }

        // Check if output is grid-based format (like Qengineering implementation)
        bool is_grid_based = false;
        int grid_h = 0, grid_w = 0;
        if (output_attr.n_dims >= 3) {
            // Grid-based format: [batch, channels, grid_h, grid_w] or [batch, grid_h, grid_w, channels]
            if (output_attr.fmt == RKNN_TENSOR_NCHW && output_attr.n_dims == 4) {
                // NCHW: [batch, channels, grid_h, grid_w]
                grid_h = static_cast<int>(output_attr.dims[2]);
                grid_w = static_cast<int>(output_attr.dims[3]);
                int channels = static_cast<int>(output_attr.dims[1]);
                if (grid_h > 0 && grid_w > 0 && channels > 4) {
                    is_grid_based = true;
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Detected grid-based format (NCHW): grid_h=%d grid_w=%d channels=%d",
                        node_name.c_str(), grid_h, grid_w, channels));
                }
            } else if (output_attr.fmt == RKNN_TENSOR_NHWC && output_attr.n_dims == 4) {
                // NHWC: [batch, grid_h, grid_w, channels]
                grid_h = static_cast<int>(output_attr.dims[1]);
                grid_w = static_cast<int>(output_attr.dims[2]);
                int channels = static_cast<int>(output_attr.dims[3]);
                if (grid_h > 0 && grid_w > 0 && channels > 4) {
                    is_grid_based = true;
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Detected grid-based format (NHWC): grid_h=%d grid_w=%d channels=%d",
                        node_name.c_str(), grid_h, grid_w, channels));
                }
            }
        }

        if (stride <= 0 && !is_grid_based) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Unable to deduce YOLO stride from output (size=%d, classes=%d, n_dims=%d)",
                                                    node_name.c_str(),
                                                    output_size,
                                                    effective_num_classes,
                                                    output_attr.n_dims));
            if (output_attr.n_dims >= 2) {
                std::string dims_str = "[";
                for (int d = 0; d < output_attr.n_dims; ++d) {
                    if (d > 0) dims_str += ", ";
                    dims_str += std::to_string(output_attr.dims[d]);
                }
                dims_str += "]";
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Output dims: %s", node_name.c_str(), dims_str.c_str()));
            }
            return;
        }

        if (is_grid_based) {
            // Implement grid-based parsing similar to Qengineering
            return parse_yolov8_output_grid_based(output_data, output_attr, grid_h, grid_w,
                                                   parsed_num_classes, lb_params, frame_size,
                                                   boxes, scores, class_ids);
        }

        if (output_size % stride != 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] YOLO output size %d is not divisible by stride %d",
                                                    node_name.c_str(),
                                                    output_size,
                                                    stride));
        }

        const int num_boxes = output_size / stride;
        if (num_boxes <= 0) {
            return;
        }

        int batch = 1;
        if (output_attr.n_dims >= 3) {
            batch = std::max(1, static_cast<int>(output_attr.dims[0]));
        }

        int boxes_per_batch = num_boxes;
        if (batch > 0 && num_boxes >= batch) {
            boxes_per_batch = std::max(1, num_boxes / batch);
        }

        bool channel_first_layout = false;
        if (output_attr.n_dims >= 3) {
            if (output_attr.dims[1] == stride) {
                channel_first_layout = true;
            } else if (output_attr.dims[output_attr.n_dims - 1] == stride) {
                channel_first_layout = false;
            } else if (output_attr.fmt == RKNN_TENSOR_NCHW) {
                channel_first_layout = true;
            }
        }

        if (channel_first_layout && boxes_per_batch <= 0) {
            channel_first_layout = false;
        }

        const float padded_w = lb_params.padded_width > 0 ? static_cast<float>(lb_params.padded_width) : static_cast<float>(input_width);
        const float padded_h = lb_params.padded_height > 0 ? static_cast<float>(lb_params.padded_height) : static_cast<float>(input_height);
        const float inferred_scale = lb_params.scale > 1e-6f
                                         ? lb_params.scale
                                         : std::min(
                                               padded_w > 0 ? padded_w / std::max(1, lb_params.original_size.width) : 1.0f,
                                               padded_h > 0 ? padded_h / std::max(1, lb_params.original_size.height) : 1.0f);
        const float inv_scale = inferred_scale > 1e-6f ? 1.0f / inferred_scale : 1.0f;
        const float pad_left_f = static_cast<float>(lb_params.pad_left);
        const float pad_top_f = static_cast<float>(lb_params.pad_top);

        const float frame_width_f = static_cast<float>(frame_size.width);
        const float frame_height_f = static_cast<float>(frame_size.height);

        auto sigmoid = [](float x) -> float {
            return 1.0f / (1.0f + std::exp(-x));
        };

        std::vector<float> row_buffer(stride);
        auto process_candidate = [&](const float* row) {
            if (row == nullptr) {
                return;
            }

            float x_center = row[0];
            float y_center = row[1];
            float width = row[2];
            float height = row[3];

            // Debug: log raw values from model (only for first few detections)
            static int debug_count = 0;
            bool should_debug = (debug_count < 5);
            if (should_debug) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Raw model output[%d]: x_center=%.6f y_center=%.6f width=%.6f height=%.6f stride=%d",
                    node_name.c_str(), debug_count, x_center, y_center, width, height, stride));
                debug_count++;
            }

            // YOLOv8 outputs coordinates in model input space (640x640 after letterbox)
            // If values are normalized (0-1), scale them to model input size
            const float max_coord = std::max(std::max(std::fabs(x_center), std::fabs(y_center)),
                                             std::max(std::fabs(width), std::fabs(height)));
            if (should_debug) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Before scale: max_coord=%.6f padded_w=%.1f padded_h=%.1f",
                    node_name.c_str(), max_coord, padded_w, padded_h));
            }
            
            if (max_coord <= 1.5f) {
                // Normalized coordinates (0-1), scale to model input size
                if (should_debug) {
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Detected normalized coords, scaling by padded_w/h",
                        node_name.c_str()));
                }
                x_center *= padded_w;
                y_center *= padded_h;
                width *= padded_w;
                height *= padded_h;
            } else {
                if (should_debug) {
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Coords already in pixel space (not normalized)",
                        node_name.c_str()));
                }
            }
            // Otherwise, assume coordinates are already in model input space (640x640)
            
            if (should_debug) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] After scale: x_center=%.2f y_center=%.2f width=%.2f height=%.2f",
                    node_name.c_str(), x_center, y_center, width, height));
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
                if (has_explicit_class_index) {
                    best_class = static_cast<int>(std::round(class_prob));
                }
            } else {
                best_score = objectness;
            }

            if (best_score < score_threshold) {
                return;
            }

            // Debug: log conversion steps (only for first few detections)
            static int debug_convert_count = 0;
            bool should_debug_convert = (debug_convert_count < 5);
            
            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: After scale(x_c=%.2f,y_c=%.2f,w=%.2f,h=%.2f)",
                    node_name.c_str(), debug_convert_count, x_center, y_center, width, height));
            }

            // Convert from model input space (640x640) to original frame space
            // Based on Qengineering/YoloV8-NPU postprocess logic
            // Step 1: Convert center+size to top-left corner in model space
            float x1 = x_center - width * 0.5f;
            float y1 = y_center - height * 0.5f;
            float x2 = x_center + width * 0.5f;
            float y2 = y_center + height * 0.5f;

            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: Step1 model_box(x1=%.2f,y1=%.2f,x2=%.2f,y2=%.2f) padded_w=%.1f padded_h=%.1f",
                    node_name.c_str(), debug_convert_count, x1, y1, x2, y2, padded_w, padded_h));
            }

            // Step 2: Clamp to model input bounds (0 to model_in_w/h)
            x1 = std::max(0.0f, std::min(x1, padded_w));
            y1 = std::max(0.0f, std::min(y1, padded_h));
            x2 = std::max(0.0f, std::min(x2, padded_w));
            y2 = std::max(0.0f, std::min(y2, padded_h));

            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: Step2 after clamp(x1=%.2f,y1=%.2f,x2=%.2f,y2=%.2f)",
                    node_name.c_str(), debug_convert_count, x1, y1, x2, y2));
            }

            // Step 3: Remove padding offset (convert from padded to resized image space)
            x1 = x1 - pad_left_f;
            y1 = y1 - pad_top_f;
            x2 = x2 - pad_left_f;
            y2 = y2 - pad_top_f;

            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: Step3 unpadded(x1=%.2f,y1=%.2f,x2=%.2f,y2=%.2f) pad(L=%d,T=%d)",
                    node_name.c_str(), debug_convert_count, x1, y1, x2, y2,
                    lb_params.pad_left, lb_params.pad_top));
            }

            // Step 4: Scale back to original frame size
            // inferred_scale is the scale from original to resized (keeping aspect ratio)
            // So to convert from resized to original, we divide by inferred_scale
            if (inferred_scale > 1e-6f) {
                x1 = x1 / inferred_scale;
                y1 = y1 / inferred_scale;
                x2 = x2 / inferred_scale;
                y2 = y2 / inferred_scale;
            }

            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: Step4 after scale(x1=%.2f,y1=%.2f,x2=%.2f,y2=%.2f) scale=%.4f",
                    node_name.c_str(), debug_convert_count, x1, y1, x2, y2, inferred_scale));
            }

            // Step 5: Convert to integer coordinates
            int x = static_cast<int>(std::round(x1));
            int y = static_cast<int>(std::round(y1));
            int w = static_cast<int>(std::round(x2 - x1));
            int h = static_cast<int>(std::round(y2 - y1));

            if (should_debug_convert) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Convert[%d]: Final(x=%d,y=%d,w=%d,h=%d) orig_size=%dx%d",
                    node_name.c_str(), debug_convert_count, x, y, w, h,
                    lb_params.original_size.width, lb_params.original_size.height));
                debug_convert_count++;
            }

            x = std::max(0, std::min(x, static_cast<int>(frame_width_f) - 1));
            y = std::max(0, std::min(y, static_cast<int>(frame_height_f) - 1));
            w = std::max(1, std::min(w, static_cast<int>(frame_width_f) - x));
            h = std::max(1, std::min(h, static_cast<int>(frame_height_f) - y));

            boxes.emplace_back(x, y, w, h);
            scores.emplace_back(best_score);
            class_ids.emplace_back(best_class);
        };

        if (channel_first_layout) {
            const int channels = stride;
            const int total_candidates = boxes_per_batch;
            for (int b = 0; b < batch; ++b) {
                const float* batch_ptr = output_data + b * channels * total_candidates;
                for (int candidate = 0; candidate < total_candidates; ++candidate) {
                    for (int c = 0; c < channels; ++c) {
                        const float* channel_ptr = batch_ptr + c * total_candidates;
                        row_buffer[c] = channel_ptr[candidate];
                    }
                    process_candidate(row_buffer.data());
                }
            }
        } else {
            for (int i = 0; i < num_boxes; ++i) {
                const float* row = output_data + i * stride;
                process_candidate(row);
            }
        }
    }

    void cvedix_rknn_yolov8_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Empty outputs or frame meta", node_name.c_str()));
            
            return;
        }

        auto& frame_meta = frame_meta_with_batch[0];
        cv::Mat& frame = frame_meta->frame;

        // Check number of outputs - Qengineering uses separate box and score outputs
        int num_outputs = static_cast<int>(raw_outputs.size());
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Number of outputs: %d", node_name.c_str(), num_outputs));
        
        if (num_outputs == 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] No outputs available", node_name.c_str()));
            return;
        }

        // Log all output shapes for debugging
        static bool logged_output_shapes = false;
        if (!logged_output_shapes) {
            for (int i = 0; i < num_outputs; ++i) {
                auto attr = rknn_helper->get_output_attr(i);
                std::string dims_str = "[";
                for (int d = 0; d < attr.n_dims; ++d) {
                    if (d > 0) dims_str += ", ";
                    dims_str += std::to_string(attr.dims[d]);
                }
                dims_str += "]";
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] Output[%d]: n_dims=%d dims=%s fmt=%d type=%d",
                    node_name.c_str(), i, attr.n_dims, dims_str.c_str(), attr.fmt, attr.type));
            }
            logged_output_shapes = true;
        }

        // Try Qengineering structure first (multiple separate outputs)
        int output_per_branch = num_outputs / 3;
        bool use_qengineering_style = (output_per_branch >= 2 && num_outputs >= 6);
        
        if (use_qengineering_style) {
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Using Qengineering structure: %d outputs, %d per branch",
                node_name.c_str(), num_outputs, output_per_branch));
        }

        const cv::Mat& output = raw_outputs[0];
        const float* output_data = output.ptr<float>();
        const int output_size = static_cast<int>(output.total());
        auto output_attr = rknn_helper->get_output_attr(0);

        // Debug: log output tensor shape and format
        static bool logged_output_info = false;
        if (!logged_output_info) {
            std::string dims_str = "[";
            for (int d = 0; d < output_attr.n_dims; ++d) {
                if (d > 0) dims_str += ", ";
                dims_str += std::to_string(output_attr.dims[d]);
            }
            dims_str += "]";
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Output tensor: n_dims=%d dims=%s fmt=%d type=%d size=%d total_elements=%d",
                node_name.c_str(), output_attr.n_dims, dims_str.c_str(),
                output_attr.fmt, output_attr.type, output_attr.size, output_size));
            
            // Log first few values to understand format
            int log_count = std::min(20, output_size);
            std::string values_str = "First " + std::to_string(log_count) + " values: ";
            for (int i = 0; i < log_count; ++i) {
                if (i > 0) values_str += ", ";
                values_str += std::to_string(output_data[i]);
            }
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] %s", node_name.c_str(), values_str.c_str()));
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
        
        // Use Qengineering structure if applicable
        if (use_qengineering_style) {
            parse_yolov8_output_qengineering_style(
                raw_outputs,
                effective_num_classes,
                lb_params,
                frame.size(),
                boxes,
                scores,
                class_ids);
        } else {
            // Fall back to single output parsing
            parse_yolov8_output(output_data,
                                output_size,
                                output_attr,
                                effective_num_classes,
                                lb_params,
                                frame.size(),
                                boxes,
                                scores,
                                class_ids);
        }

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

    void cvedix_rknn_yolov8_detector_node::parse_yolov8_output_grid_based(
        const float* output_data,
        const rknn_tensor_attr& output_attr,
        int grid_h,
        int grid_w,
        int parsed_num_classes,
        const letterbox_params& lb_params,
        const cv::Size& frame_size,
        std::vector<cv::Rect>& boxes,
        std::vector<float>& scores,
        std::vector<int>& class_ids) {
        
        boxes.clear();
        scores.clear();
        class_ids.clear();

        if (output_data == nullptr || grid_h <= 0 || grid_w <= 0) {
            return;
        }

        const int effective_num_classes = parsed_num_classes > 0 ? parsed_num_classes : 1;
        const int grid_len = grid_h * grid_w;
        
        // Calculate stride from grid size
        const float padded_w = lb_params.padded_width > 0 ? static_cast<float>(lb_params.padded_width) : static_cast<float>(input_width);
        const float padded_h = lb_params.padded_height > 0 ? static_cast<float>(lb_params.padded_height) : static_cast<float>(input_height);
        const int stride = static_cast<int>(padded_h / grid_h);
        
        // Determine channels and layout
        int channels = 0;
        bool is_nchw = (output_attr.fmt == RKNN_TENSOR_NCHW);
        
        if (is_nchw && output_attr.n_dims == 4) {
            // NCHW: [batch, channels, grid_h, grid_w]
            channels = static_cast<int>(output_attr.dims[1]);
        } else if (!is_nchw && output_attr.n_dims == 4) {
            // NHWC: [batch, grid_h, grid_w, channels]
            channels = static_cast<int>(output_attr.dims[3]);
        } else {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Unsupported grid-based format: n_dims=%d fmt=%d",
                                                    node_name.c_str(), output_attr.n_dims, output_attr.fmt));
            return;
        }

        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Grid-based parsing: grid_h=%d grid_w=%d channels=%d stride=%d is_nchw=%d",
            node_name.c_str(), grid_h, grid_w, channels, stride, is_nchw));

        // Determine box and score channel ranges
        // Typical YOLOv8 format: [box_channels(4 or dfl_len*4), score_channels(num_classes)]
        // Or combined: [x, y, w, h, obj, class1, class2, ...]
        int box_channels = 4;  // Assume 4 box channels (x, y, w, h) or could be dfl_len*4
        int score_start_channel = box_channels;
        
        // Check if we have objectness channel
        bool has_objectness = (channels >= box_channels + 1 + effective_num_classes);
        if (has_objectness) {
            score_start_channel = box_channels + 1;  // Skip objectness
        }

        auto sigmoid = [](float x) -> float {
            return 1.0f / (1.0f + std::exp(-x));
        };

        const float inferred_scale = lb_params.scale > 1e-6f
                                         ? lb_params.scale
                                         : std::min(
                                               padded_w > 0 ? padded_w / std::max(1, lb_params.original_size.width) : 1.0f,
                                               padded_h > 0 ? padded_h / std::max(1, lb_params.original_size.height) : 1.0f);
        const float pad_left_f = static_cast<float>(lb_params.pad_left);
        const float pad_top_f = static_cast<float>(lb_params.pad_top);

        // Parse each grid cell
        for (int i = 0; i < grid_h; ++i) {
            for (int j = 0; j < grid_w; ++j) {
                int grid_offset = i * grid_w + j;
                
                // Get box data
                float box_data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                if (is_nchw) {
                    // NCHW: data[channel * grid_len + grid_offset]
                    for (int c = 0; c < 4 && c < box_channels; ++c) {
                        int channel_offset = c * grid_len + grid_offset;
                        if (channel_offset < channels * grid_len) {
                            box_data[c] = output_data[channel_offset];
                        }
                    }
                } else {
                    // NHWC: data[(i * grid_w + j) * channels + channel]
                    int base_offset = grid_offset * channels;
                    for (int c = 0; c < 4 && c < box_channels; ++c) {
                        if (base_offset + c < grid_len * channels) {
                            box_data[c] = output_data[base_offset + c];
                        }
                    }
                }

                // Debug: log first few grid cells with all channel data
                static int debug_grid_count = 0;
                bool should_debug = (debug_grid_count < 10 && (i < 3 || i == grid_h/2) && (j < 3 || j == grid_w/2));
                if (should_debug) {
                    std::string channels_str = "channels=[";
                    int log_channels = std::min(8, channels);
                    if (is_nchw) {
                        for (int c = 0; c < log_channels; ++c) {
                            if (c > 0) channels_str += ",";
                            int offset = c * grid_len + grid_offset;
                            if (offset < channels * grid_len) {
                                channels_str += std::to_string(output_data[offset]);
                            }
                        }
                    } else {
                        int base_offset = grid_offset * channels;
                        for (int c = 0; c < log_channels; ++c) {
                            if (c > 0) channels_str += ",";
                            int offset = base_offset + c;
                            if (offset < grid_len * channels) {
                                channels_str += std::to_string(output_data[offset]);
                            }
                        }
                    }
                    channels_str += "]";
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Grid[%d,%d] box_data=[%.4f,%.4f,%.4f,%.4f] %s",
                        node_name.c_str(), i, j, box_data[0], box_data[1], box_data[2], box_data[3], channels_str.c_str()));
                    debug_grid_count++;
                }

                // Get score data
                float max_score = 0.0f;
                int best_class = -1;
                float objectness = 1.0f;

                if (has_objectness && box_channels < channels) {
                    int obj_channel = box_channels;
                    if (is_nchw) {
                        int obj_offset = obj_channel * grid_len + grid_offset;
                        if (obj_offset < channels * grid_len) {
                            objectness = sigmoid(output_data[obj_offset]);
                        }
                    } else {
                        int obj_offset = grid_offset * channels + obj_channel;
                        if (obj_offset < grid_len * channels) {
                            objectness = sigmoid(output_data[obj_offset]);
                        }
                    }
                }

                // Find best class
                for (int c = 0; c < effective_num_classes && (score_start_channel + c) < channels; ++c) {
                    float class_score = 0.0f;
                    int score_channel = score_start_channel + c;
                    
                    if (is_nchw) {
                        int score_offset = score_channel * grid_len + grid_offset;
                        if (score_offset < channels * grid_len) {
                            class_score = sigmoid(output_data[score_offset]);
                        }
                    } else {
                        int score_offset = grid_offset * channels + score_channel;
                        if (score_offset < grid_len * channels) {
                            class_score = sigmoid(output_data[score_offset]);
                        }
                    }
                    
                    float final_score = objectness * class_score;
                    if (final_score > max_score) {
                        max_score = final_score;
                        best_class = c;
                    }
                }

                if (max_score < score_threshold || best_class < 0) {
                    continue;
                }

                // Convert box from grid space to pixel space
                // YOLOv8 grid format (like Qengineering): 
                // box[0] = left offset (negative), box[1] = top offset (negative)
                // box[2] = right offset (positive), box[3] = bottom offset (positive)
                // Or could be center+size format: box[0]=x_center, box[1]=y_center, box[2]=width, box[3]=height
                
                float x1, y1, x2, y2;
                
                // Try Qengineering format first (offset-based)
                // Check if values look like offsets (typically small, can be negative)
                const float max_abs_val = std::max(std::max(std::fabs(box_data[0]), std::fabs(box_data[1])),
                                                   std::max(std::fabs(box_data[2]), std::fabs(box_data[3])));
                
                if (max_abs_val <= 5.0f && (box_data[0] < 0 || box_data[1] < 0 || box_data[2] > 0 || box_data[3] > 0)) {
                    // Qengineering format: offset-based
                    x1 = (-box_data[0] + j + 0.5f) * stride;
                    y1 = (-box_data[1] + i + 0.5f) * stride;
                    x2 = (box_data[2] + j + 0.5f) * stride;
                    y2 = (box_data[3] + i + 0.5f) * stride;
                } else {
                    // Center+size format: box[0]=x_center, box[1]=y_center, box[2]=width, box[3]=height
                    // Check if normalized
                    if (max_abs_val <= 1.5f) {
                        // Normalized: scale to model input size
                        float x_center = box_data[0] * padded_w;
                        float y_center = box_data[1] * padded_h;
                        float width = box_data[2] * padded_w;
                        float height = box_data[3] * padded_h;
                        x1 = x_center - width * 0.5f;
                        y1 = y_center - height * 0.5f;
                        x2 = x_center + width * 0.5f;
                        y2 = y_center + height * 0.5f;
                    } else {
                        // Already in pixel space
                        float x_center = box_data[0];
                        float y_center = box_data[1];
                        float width = box_data[2];
                        float height = box_data[3];
                        x1 = x_center - width * 0.5f;
                        y1 = y_center - height * 0.5f;
                        x2 = x_center + width * 0.5f;
                        y2 = y_center + height * 0.5f;
                    }
                }

                // Clamp to model input bounds
                x1 = std::max(0.0f, std::min(x1, padded_w));
                y1 = std::max(0.0f, std::min(y1, padded_h));
                x2 = std::max(0.0f, std::min(x2, padded_w));
                y2 = std::max(0.0f, std::min(y2, padded_h));

                // Remove padding
                x1 = x1 - pad_left_f;
                y1 = y1 - pad_top_f;
                x2 = x2 - pad_left_f;
                y2 = y2 - pad_top_f;

                // Scale to original frame size
                if (inferred_scale > 1e-6f) {
                    x1 = x1 / inferred_scale;
                    y1 = y1 / inferred_scale;
                    x2 = x2 / inferred_scale;
                    y2 = y2 / inferred_scale;
                }

                // Convert to integer coordinates
                int x = static_cast<int>(std::round(x1));
                int y = static_cast<int>(std::round(y1));
                int w = static_cast<int>(std::round(x2 - x1));
                int h = static_cast<int>(std::round(y2 - y1));

                // Clamp to frame bounds
                x = std::max(0, std::min(x, frame_size.width - 1));
                y = std::max(0, std::min(y, frame_size.height - 1));
                w = std::max(1, std::min(w, frame_size.width - x));
                h = std::max(1, std::min(h, frame_size.height - y));

                // Filter out boxes that are too small (likely false positives)
                const int min_box_size = 10;  // Minimum width or height
                const float min_aspect_ratio = 0.1f;  // Minimum w/h or h/w ratio
                const float max_aspect_ratio = 10.0f;  // Maximum w/h or h/w ratio
                
                if (w < min_box_size && h < min_box_size) {
                    // Skip boxes that are too small in both dimensions
                    continue;
                }
                
                float aspect_ratio = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 0.0f;
                if (aspect_ratio < min_aspect_ratio || aspect_ratio > max_aspect_ratio) {
                    // Skip boxes with extreme aspect ratios (likely false positives)
                    continue;
                }

                boxes.emplace_back(x, y, w, h);
                scores.emplace_back(max_score);
                class_ids.emplace_back(best_class);
            }
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Grid-based parsing: found %zu detections",
                                                node_name.c_str(), boxes.size()));
    }

    void cvedix_rknn_yolov8_detector_node::compute_dfl(const float* tensor, int dfl_len, float* box) {
        for (int b = 0; b < 4; b++) {
            float exp_t[dfl_len];
            float exp_sum = 0;
            float acc_sum = 0;
            for (int i = 0; i < dfl_len; i++) {
                exp_t[i] = std::exp(tensor[i + b * dfl_len]);
                exp_sum += exp_t[i];
            }

            for (int i = 0; i < dfl_len; i++) {
                acc_sum += exp_t[i] / exp_sum * i;
            }
            box[b] = acc_sum;
        }
    }

    int cvedix_rknn_yolov8_detector_node::process_fp32_branch(
        const float* box_tensor,
        const float* score_tensor,
        const float* score_sum_tensor,
        int grid_h,
        int grid_w,
        int stride,
        int dfl_len,
        int num_classes,
        float threshold,
        std::vector<float>& boxes,
        std::vector<float>& objProbs,
        std::vector<int>& classId) {
        
        int validCount = 0;
        int grid_len = grid_h * grid_w;
        
        // Debug: check first few score values
        static int debug_score_count = 0;
        bool should_debug = (debug_score_count < 10);
        
        // Statistics for debugging
        int total_cells_checked = 0;
        int cells_passed_score_sum = 0;
        int cells_with_valid_score = 0;
        float max_score_found = 0.0f;
        float max_score_sum_found = 0.0f;

        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                int offset = i * grid_w + j;
                int max_class_id = -1;
                total_cells_checked++;

                // Fast filtering using score_sum if available
                // Note: Some models may not have score_sum properly normalized, so we use it as a hint only
                bool skip_by_score_sum = false;
                if (score_sum_tensor != nullptr) {
                    float score_sum_val = score_sum_tensor[offset];
                    max_score_sum_found = std::max(max_score_sum_found, std::fabs(score_sum_val));
                    
                    // Check if score_sum needs sigmoid
                    float score_sum_normalized = score_sum_val;
                    if (score_sum_val < 0.0f || score_sum_val > 1.0f) {
                        score_sum_normalized = 1.0f / (1.0f + std::exp(-score_sum_val));
                    }
                    if (should_debug && (i < 5 || i == grid_h/2) && (j < 5 || j == grid_w/2)) {
                        CVEDIX_DEBUG(cvedix_utils::string_format(
                            "[%s] Grid[%d,%d] score_sum_raw=%.6f score_sum_norm=%.6f threshold=%.6f",
                            node_name.c_str(), i, j, score_sum_val, score_sum_normalized, threshold * 0.5f));
                    }
                    // Temporarily disable score_sum filter to see if it's too strict
                    // Use a very low threshold or disable completely for debugging
                    if (score_sum_normalized < threshold * 0.1f) {
                        skip_by_score_sum = true;
                    } else {
                        cells_passed_score_sum++;
                    }
                } else {
                    cells_passed_score_sum++;
                }
                
                if (skip_by_score_sum) {
                    continue;
                }

                float max_score = 0.0f;
                int base_offset = offset;
                for (int c = 0; c < num_classes; c++) {
                    float score_val = score_tensor[offset];
                    max_score_found = std::max(max_score_found, std::fabs(score_val));
                    
                    // Try both with and without sigmoid
                    // Some models output raw logits, others output normalized scores
                    float score_normalized = score_val;
                    if (score_val < 0.0f || score_val > 1.0f) {
                        // Likely raw logits, apply sigmoid
                        score_normalized = 1.0f / (1.0f + std::exp(-score_val));
                    }
                    
                    if (should_debug && (i < 5 || i == grid_h/2) && (j < 5 || j == grid_w/2) && c < 3) {
                        CVEDIX_DEBUG(cvedix_utils::string_format(
                            "[%s] Grid[%d,%d] class[%d] score_raw=%.6f score_norm=%.6f threshold=%.6f",
                            node_name.c_str(), i, j, c, score_val, score_normalized, threshold));
                    }
                    
                    if (score_normalized > threshold && score_normalized > max_score) {
                        max_score = score_normalized;
                        max_class_id = c;
                    }
                    offset += grid_len;
                }
                
                if (max_score > 0.0f) {
                    cells_with_valid_score++;
                }
                
                if (should_debug && (i < 5 || i == grid_h/2) && (j < 5 || j == grid_w/2)) {
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Grid[%d,%d] max_score=%.6f max_class=%d threshold=%.6f",
                        node_name.c_str(), i, j, max_score, max_class_id, threshold));
                    if (i == 4 && j == 4) {
                        debug_score_count++;
                    }
                }

                // Compute box
                if (max_score > threshold && max_class_id >= 0) {
                    offset = i * grid_w + j;
                    float box[4];
                    float before_dfl[dfl_len * 4];
                    for (int k = 0; k < dfl_len * 4; k++) {
                        before_dfl[k] = box_tensor[offset];
                        offset += grid_len;
                    }
                    compute_dfl(before_dfl, dfl_len, box);

                    float x1, y1, x2, y2, w, h;
                    x1 = (-box[0] + j + 0.5f) * stride;
                    y1 = (-box[1] + i + 0.5f) * stride;
                    x2 = (box[2] + j + 0.5f) * stride;
                    y2 = (box[3] + i + 0.5f) * stride;
                    w = x2 - x1;
                    h = y2 - y1;
                    boxes.push_back(x1);
                    boxes.push_back(y1);
                    boxes.push_back(w);
                    boxes.push_back(h);

                    objProbs.push_back(max_score);
                    classId.push_back(max_class_id);
                    validCount++;
                }
            }
        }
        
        // Debug statistics
        if (should_debug || validCount == 0) {
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Branch stats: total_cells=%d passed_score_sum=%d valid_scores=%d valid_detections=%d max_score_found=%.6f max_score_sum=%.6f threshold=%.6f",
                node_name.c_str(), total_cells_checked, cells_passed_score_sum, cells_with_valid_score, 
                validCount, max_score_found, max_score_sum_found, threshold));
        }
        
        return validCount;
    }

    void cvedix_rknn_yolov8_detector_node::parse_yolov8_output_qengineering_style(
        const std::vector<cv::Mat>& raw_outputs,
        int parsed_num_classes,
        const letterbox_params& lb_params,
        const cv::Size& frame_size,
        std::vector<cv::Rect>& boxes,
        std::vector<float>& scores,
        std::vector<int>& class_ids) {
        
        boxes.clear();
        scores.clear();
        class_ids.clear();

        int num_outputs = static_cast<int>(raw_outputs.size());
        if (num_outputs == 0) {
            return;
        }

        const int effective_num_classes = parsed_num_classes > 0 ? parsed_num_classes : 80;
        const float padded_w = lb_params.padded_width > 0 ? static_cast<float>(lb_params.padded_width) : static_cast<float>(input_width);
        const float padded_h = lb_params.padded_height > 0 ? static_cast<float>(lb_params.padded_height) : static_cast<float>(input_height);
        const float inferred_scale = lb_params.scale > 1e-6f
                                         ? lb_params.scale
                                         : std::min(
                                               padded_w > 0 ? padded_w / std::max(1, lb_params.original_size.width) : 1.0f,
                                               padded_h > 0 ? padded_h / std::max(1, lb_params.original_size.height) : 1.0f);

        // Qengineering structure: default 3 branches
        // Each branch has: box output, score output, (optional) score_sum output
        int output_per_branch = num_outputs / 3;
        if (output_per_branch < 2) {
            // Not enough outputs for Qengineering structure, fall back to single output
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Not enough outputs (%d) for Qengineering structure, using single output",
                node_name.c_str(), num_outputs));
            return;
        }

        // Calculate DFL length from first box output
        auto box_attr_0 = rknn_helper->get_output_attr(0);
        int dfl_len = 1;
        if (box_attr_0.n_dims >= 2 && box_attr_0.dims[1] > 0) {
            dfl_len = box_attr_0.dims[1] / 4;
        }

        std::vector<float> filterBoxes;
        std::vector<float> objProbs;
        std::vector<int> classId;
        int total_valid_count = 0;

        // Process each branch
        for (int branch = 0; branch < 3; branch++) {
            int box_idx = branch * output_per_branch;
            int score_idx = branch * output_per_branch + 1;
            
            if (box_idx >= num_outputs || score_idx >= num_outputs) {
                continue;
            }

            auto box_attr = rknn_helper->get_output_attr(box_idx);
            auto score_attr = rknn_helper->get_output_attr(score_idx);

            int grid_h = 0, grid_w = 0;
            if (box_attr.n_dims >= 3) {
                if (box_attr.fmt == RKNN_TENSOR_NCHW) {
                    grid_h = static_cast<int>(box_attr.dims[2]);
                    grid_w = static_cast<int>(box_attr.dims[3]);
                } else {
                    grid_h = static_cast<int>(box_attr.dims[1]);
                    grid_w = static_cast<int>(box_attr.dims[2]);
                }
            }

            if (grid_h <= 0 || grid_w <= 0) {
                continue;
            }

            int stride = static_cast<int>(padded_h / grid_h);
            if (stride <= 0) {
                stride = static_cast<int>(padded_w / grid_w);
            }

            const cv::Mat& box_mat = raw_outputs[box_idx];
            const cv::Mat& score_mat = raw_outputs[score_idx];
            const float* box_tensor = box_mat.ptr<float>();
            const float* score_tensor = score_mat.ptr<float>();

            const float* score_sum_tensor = nullptr;
            if (output_per_branch >= 3) {
                int score_sum_idx = branch * output_per_branch + 2;
                if (score_sum_idx < num_outputs) {
                    const cv::Mat& score_sum_mat = raw_outputs[score_sum_idx];
                    score_sum_tensor = score_sum_mat.ptr<float>();
                }
            }

            int branch_count = process_fp32_branch(
                box_tensor, score_tensor, score_sum_tensor,
                grid_h, grid_w, stride, dfl_len, effective_num_classes,
                score_threshold, filterBoxes, objProbs, classId);
            
            total_valid_count += branch_count;
            
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Branch[%d]: grid_h=%d grid_w=%d stride=%d dfl_len=%d valid=%d",
                node_name.c_str(), branch, grid_h, grid_w, stride, dfl_len, branch_count));
        }

        if (total_valid_count <= 0) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] No valid detections from Qengineering parsing",
                                                    node_name.c_str()));
            return;
        }

        // Convert boxes from model space to original frame space
        for (int i = 0; i < total_valid_count; ++i) {
            float x1 = filterBoxes[i * 4 + 0];
            float y1 = filterBoxes[i * 4 + 1];
            float w = filterBoxes[i * 4 + 2];
            float h = filterBoxes[i * 4 + 3];
            float x2 = x1 + w;
            float y2 = y1 + h;

            // Clamp to model input bounds
            x1 = std::max(0.0f, std::min(x1, padded_w));
            y1 = std::max(0.0f, std::min(y1, padded_h));
            x2 = std::max(0.0f, std::min(x2, padded_w));
            y2 = std::max(0.0f, std::min(y2, padded_h));

            // Remove padding
            const float pad_left_f = static_cast<float>(lb_params.pad_left);
            const float pad_top_f = static_cast<float>(lb_params.pad_top);
            x1 = x1 - pad_left_f;
            y1 = y1 - pad_top_f;
            x2 = x2 - pad_left_f;
            y2 = y2 - pad_top_f;

            // Scale to original frame size
            if (inferred_scale > 1e-6f) {
                x1 = x1 / inferred_scale;
                y1 = y1 / inferred_scale;
                x2 = x2 / inferred_scale;
                y2 = y2 / inferred_scale;
            }

            // Convert to integer coordinates
            int x = static_cast<int>(std::round(x1));
            int y = static_cast<int>(std::round(y1));
            int width = static_cast<int>(std::round(x2 - x1));
            int height = static_cast<int>(std::round(y2 - y1));

            // Clamp to frame bounds
            x = std::max(0, std::min(x, frame_size.width - 1));
            y = std::max(0, std::min(y, frame_size.height - 1));
            width = std::max(1, std::min(width, frame_size.width - x));
            height = std::max(1, std::min(height, frame_size.height - y));

            // Filter out boxes that are too small
            const int min_box_size = 10;
            const float min_aspect_ratio = 0.1f;
            const float max_aspect_ratio = 10.0f;
            
            if (width < min_box_size && height < min_box_size) {
                continue;
            }
            
            float aspect_ratio = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 0.0f;
            if (aspect_ratio < min_aspect_ratio || aspect_ratio > max_aspect_ratio) {
                continue;
            }

            boxes.emplace_back(x, y, width, height);
            scores.push_back(objProbs[i]);
            class_ids.push_back(classId[i]);
        }

        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Qengineering parsing: found %zu detections",
                                                node_name.c_str(), boxes.size()));
    }

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_RKNN

