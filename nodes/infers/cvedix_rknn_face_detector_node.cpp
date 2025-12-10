#include "cvedix_rknn_face_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {

    cvedix_rknn_face_detector_node::cvedix_rknn_face_detector_node(
        std::string node_name,
        std::string model_path,
        float score_threshold,
        float nms_threshold,
        int top_k,
        int input_width,
        int input_height)
        : cvedix_primary_infer_node(node_name,
                                    model_path,
                                    "",
                                    "",
                                    input_width,
                                    input_height,
                                    1),
          score_threshold(score_threshold),
          nms_threshold(nms_threshold),
          top_k(top_k),
          input_width(input_width),
          input_height(input_height),
          current_input_w(0),
          current_input_h(0) {

        #ifdef CVEDIX_WITH_LICENSE
        if (!cvedix_utils::cvedix_license_manager::get_instance().check_license()) {
            throw std::runtime_error("RKNN features require a valid license. Please contact support.");
        }
        #endif

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
        
        // Initialize priors for the default input size
        generate_priors(input_width, input_height);

        this->initialized();
    }

    cvedix_rknn_face_detector_node::~cvedix_rknn_face_detector_node() {
        deinitialized();
    }

    void cvedix_rknn_face_detector_node::sync_model_input_shape() {
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

    void cvedix_rknn_face_detector_node::generate_priors(int input_w, int input_h) {
        using namespace cv;
        
        // Only regenerate if size changed
        if (input_w == current_input_w && input_h == current_input_h && !priors.empty()) {
            return;
        }
        
        current_input_w = input_w;
        current_input_h = input_h;
        priors.clear();
        
        // Calculate shapes of different scales according to the shape of input image
        // Same logic as YuNet
        Size feature_map_2nd = {
            int(int((input_w+1)/2)/2), int(int((input_h+1)/2)/2)
        };
        Size feature_map_3rd = {
            int(feature_map_2nd.width/2), int(feature_map_2nd.height/2)
        };
        Size feature_map_4th = {
            int(feature_map_3rd.width/2), int(feature_map_3rd.height/2)
        };
        Size feature_map_5th = {
            int(feature_map_4th.width/2), int(feature_map_4th.height/2)
        };
        Size feature_map_6th = {
            int(feature_map_5th.width/2), int(feature_map_5th.height/2)
        };

        std::vector<Size> feature_map_sizes;
        feature_map_sizes.push_back(feature_map_3rd);
        feature_map_sizes.push_back(feature_map_4th);
        feature_map_sizes.push_back(feature_map_5th);
        feature_map_sizes.push_back(feature_map_6th);

        // Fixed params for generating priors (same as YuNet)
        const std::vector<std::vector<float>> min_sizes = {
            {10.0f,  16.0f,  24.0f},
            {32.0f,  48.0f},
            {64.0f,  96.0f},
            {128.0f, 192.0f, 256.0f}
        };
        CV_Assert(min_sizes.size() == feature_map_sizes.size());
        const std::vector<int> steps = { 8, 16, 32, 64 };

        // Generate priors
        for (size_t i = 0; i < feature_map_sizes.size(); ++i) {
            Size feature_map_size = feature_map_sizes[i];
            std::vector<float> min_size = min_sizes[i];

            for (int _h = 0; _h < feature_map_size.height; ++_h) {
                for (int _w = 0; _w < feature_map_size.width; ++_w) {
                    for (size_t j = 0; j < min_size.size(); ++j) {
                        float s_kx = min_size[j] / input_w;
                        float s_ky = min_size[j] / input_h;

                        float cx = (_w + 0.5f) * steps[i] / input_w;
                        float cy = (_h + 0.5f) * steps[i] / input_h;

                        Rect2f prior = { cx, cy, s_kx, s_ky };
                        priors.push_back(prior);
                    }
                }
            }
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Generated %zu priors for input size %dx%d",
                                                 node_name.c_str(), priors.size(), input_w, input_h));
    }

    void cvedix_rknn_face_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer,
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

        // Calculate letterbox parameters
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

        // Create padded image with gray background (114, 114, 114)
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

        {
            std::lock_guard<std::mutex> guard(letterbox_mutex);
            pending_letterbox_params.push_back(params);
        }

        // Determine input requirements
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

    void cvedix_rknn_face_detector_node::infer(const cv::Mat& blob_to_infer,
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
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] RKNN inference failed, ret=%d", node_name.c_str(), ret));
            return;
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] RKNN inference completed successfully", node_name.c_str()));

        int tensor_count = rknn_helper->get_output_num();
        raw_outputs.clear();
        raw_outputs.reserve(tensor_count);

        for (int i = 0; i < tensor_count; ++i) {
            auto output_attr = rknn_helper->get_output_attr(i);
            std::vector<int> shape;
            for (int d = 0; d < output_attr.n_dims; ++d) {
                shape.push_back(output_attr.dims[d]);
            }
            
            // Log output shape for debugging
            std::string dims_str = "[";
            for (size_t d = 0; d < shape.size(); ++d) {
                if (d > 0) dims_str += ",";
                dims_str += std::to_string(shape[d]);
            }
            dims_str += "]";
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Output[%d] shape: n_dims=%d dims=%s",
                                                     node_name.c_str(), i, output_attr.n_dims, dims_str.c_str()));
            
            cv::Mat output_mat = rknn_helper->get_output_mat(i, shape);
            if (!output_mat.empty()) {
                // YuNet outputs are typically 2D: [num_priors, features]
                // But RKNN might return them as 1D or with different dimensions
                // We need to reshape them properly
                int total_elements = output_mat.total();
                
                if (shape.size() == 2) {
                    // Already 2D: [num_priors, features]
                    // Just ensure it's properly shaped
                    output_mat = output_mat.reshape(1, shape[0]);
                    // Ensure contiguous for safe pointer arithmetic
                    if (!output_mat.isContinuous()) {
                        output_mat = output_mat.clone();
                    }
                } else if (shape.size() == 1) {
                    // 1D output, need to infer the correct shape
                    // For YuNet:
                    // - loc: [num_priors * 14] -> reshape to [num_priors, 14]
                    // - conf: [num_priors * 2] -> reshape to [num_priors, 2]
                    // - iou: [num_priors] -> reshape to [num_priors, 1]
                    if (i == 0 && total_elements % 14 == 0) {
                        // loc output: reshape to [num_priors, 14]
                        int num_priors = total_elements / 14;
                        output_mat = output_mat.reshape(1, num_priors);
                        // Ensure contiguous for safe pointer arithmetic
                        if (!output_mat.isContinuous()) {
                            output_mat = output_mat.clone();
                        }
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Reshaped loc output from 1D[%d] to 2D[%d, 14]",
                                                                 node_name.c_str(), total_elements, num_priors));
                    } else if (i == 1 && total_elements % 2 == 0) {
                        // conf output: reshape to [num_priors, 2]
                        int num_priors = total_elements / 2;
                        output_mat = output_mat.reshape(1, num_priors);
                        // Ensure contiguous for safe pointer arithmetic
                        if (!output_mat.isContinuous()) {
                            output_mat = output_mat.clone();
                        }
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Reshaped conf output from 1D[%d] to 2D[%d, 2]",
                                                                 node_name.c_str(), total_elements, num_priors));
                    } else if (i == 2) {
                        // iou output: reshape to [num_priors, 1]
                        output_mat = output_mat.reshape(1, total_elements);
                        // Ensure contiguous for safe pointer arithmetic
                        if (!output_mat.isContinuous()) {
                            output_mat = output_mat.clone();
                        }
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Reshaped iou output from 1D[%d] to 2D[%d, 1]",
                                                                 node_name.c_str(), total_elements, total_elements));
                    }
                } else if (shape.size() >= 3) {
                    // Higher dimensional output, flatten and reshape
                    // Calculate num_priors from first dimension (excluding batch if present)
                    int num_priors = shape[0];
                    if (num_priors == 1 && shape.size() > 1) {
                        num_priors = shape[1];
                    }
                    int features = total_elements / num_priors;
                    if (features > 0 && total_elements % num_priors == 0) {
                        output_mat = output_mat.reshape(1, num_priors);
                        // Ensure contiguous for safe pointer arithmetic
                        if (!output_mat.isContinuous()) {
                            output_mat = output_mat.clone();
                        }
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Reshaped output[%d] from %dD to 2D[%d, %d]",
                                                                 node_name.c_str(), i, (int)shape.size(), num_priors, features));
                    }
                }
                
                raw_outputs.push_back(output_mat);
            } else {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Failed to get output[%d]", node_name.c_str(), i));
            }
        }
        
        // YuNet expects 3 outputs: loc, conf, iou
        if (raw_outputs.size() < 3) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Expected 3 outputs (loc, conf, iou) but got %zu",
                                                    node_name.c_str(), raw_outputs.size()));
        } else {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Got %zu outputs: loc[%dx%d] conf[%dx%d] iou[%dx%d]",
                                                     node_name.c_str(),
                                                     raw_outputs.size(),
                                                     raw_outputs[0].rows, raw_outputs[0].cols,
                                                     raw_outputs[1].rows, raw_outputs[1].cols,
                                                     raw_outputs[2].rows, raw_outputs[2].cols));
            
            // Log statistics về conf và iou để debug
            if (!raw_outputs[1].empty() && raw_outputs[1].type() == CV_32F) {
                cv::Mat conf_mat = raw_outputs[1];
                // Conf has shape [num_priors, 2] - [bg_score, face_score]
                if (conf_mat.cols >= 2) {
                    cv::Mat bg_col = conf_mat.col(0);
                    cv::Mat face_col = conf_mat.col(1);
                    
                    double bg_min, bg_max, face_min, face_max;
                    cv::Point bg_min_loc, bg_max_loc, face_min_loc, face_max_loc;
                    cv::minMaxLoc(bg_col, &bg_min, &bg_max, &bg_min_loc, &bg_max_loc);
                    cv::minMaxLoc(face_col, &face_min, &face_max, &face_min_loc, &face_max_loc);
                    
                    cv::Scalar bg_mean = cv::mean(bg_col);
                    cv::Scalar face_mean = cv::mean(face_col);
                    
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Conf stats - bg: [%.4f, %.4f] mean=%.4f, face: [%.4f, %.4f] mean=%.4f",
                                                             node_name.c_str(), 
                                                             bg_min, bg_max, bg_mean[0],
                                                             face_min, face_max, face_mean[0]));
                    
                    // Sample first few values
                    if (conf_mat.rows > 0) {
                        int sample_count = std::min(5, conf_mat.rows);
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Sample conf values (first %d):", node_name.c_str(), sample_count));
                        for (int i = 0; i < sample_count; ++i) {
                            float bg_val = conf_mat.at<float>(i, 0);
                            float face_val = conf_mat.at<float>(i, 1);
                            float sum = bg_val + face_val;
                            CVEDIX_DEBUG(cvedix_utils::string_format("[%s]   [%d] bg=%.4f, face=%.4f, sum=%.4f",
                                                                     node_name.c_str(), i, bg_val, face_val, sum));
                        }
                    }
                }
            }
            if (!raw_outputs[2].empty() && raw_outputs[2].type() == CV_32F) {
                cv::Mat iou_mat = raw_outputs[2];
                double min_val, max_val;
                cv::Point min_loc, max_loc;
                cv::minMaxLoc(iou_mat, &min_val, &max_val, &min_loc, &max_loc);
                cv::Scalar mean_val = cv::mean(iou_mat);
                
                // Count values outside [0, 1] range
                int out_of_range = 0;
                int total_count = iou_mat.rows * iou_mat.cols;
                for (int i = 0; i < iou_mat.rows; ++i) {
                    float val = iou_mat.at<float>(i, 0);
                    if (val < 0.0f || val > 1.0f) {
                        out_of_range++;
                    }
                }
                
                CVEDIX_INFO(cvedix_utils::string_format("[%s] IOU stats: raw [%.4f, %.4f] mean=%.4f, out_of_range=%d/%d (%.1f%%)",
                                                         node_name.c_str(), min_val, max_val, mean_val[0],
                                                         out_of_range, total_count, 
                                                         100.0f * out_of_range / total_count));
                
                // Sample first few values
                if (iou_mat.rows > 0) {
                    int sample_count = std::min(5, iou_mat.rows);
                    CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Sample IOU values (first %d):", node_name.c_str(), sample_count));
                    for (int i = 0; i < sample_count; ++i) {
                        float val = iou_mat.at<float>(i, 0);
                        float clamped = std::max(0.0f, std::min(1.0f, val));
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s]   [%d] raw=%.4f, clamped=%.4f",
                                                                 node_name.c_str(), i, val, clamped));
                    }
                }
            }
        }
    }

    void cvedix_rknn_face_detector_node::parse_multiscale_outputs(
        const std::vector<cv::Mat>& raw_outputs,
        const letterbox_params& lb_params,
        const cv::Size& frame_size,
        std::vector<cv::Rect>& boxes,
        std::vector<float>& scores,
        std::vector<std::vector<std::pair<int, int>>>& keypoints) {
        
        using namespace cv;
        
        boxes.clear();
        scores.clear();
        keypoints.clear();
        
        if (raw_outputs.size() != 12) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] parse_multiscale_outputs: Expected 12 outputs but got %zu",
                                                    node_name.c_str(), raw_outputs.size()));
            return;
        }
        
        const float padded_w = static_cast<float>(lb_params.padded_width);
        const float padded_h = static_cast<float>(lb_params.padded_height);
        const float inferred_scale = lb_params.scale > 1e-6f
                                         ? lb_params.scale
                                         : std::min(
                                               padded_w > 0 ? padded_w / std::max(1, lb_params.original_size.width) : 1.0f,
                                               padded_h > 0 ? padded_h / std::max(1, lb_params.original_size.height) : 1.0f);
        const float pad_left_f = static_cast<float>(lb_params.pad_left);
        const float pad_top_f = static_cast<float>(lb_params.pad_top);
        
        const float frame_width_f = static_cast<float>(frame_size.width);
        const float frame_height_f = static_cast<float>(frame_size.height);
        
        // YuNet variance parameters (same as cvedix_yunet_face_detector_node.cpp)
        const std::vector<float> variance = {0.1f, 0.2f};
        
        // Generate priors for multi-scale model if needed
        if (priors.empty() || current_input_w != lb_params.padded_width || current_input_h != lb_params.padded_height) {
            generate_priors(lb_params.padded_width, lb_params.padded_height);
        }
        
        // Multi-scale structure: 3 scales
        // Based on output shapes:
        // Output[0]: 6400x1 (conf scale 0 - bg)
        // Output[1]: 1600x1 (conf scale 1 - bg)
        // Output[2]: 400x1  (conf scale 2 - bg)
        // Output[3]: 6400x1 (conf scale 0 - face)
        // Output[4]: 1600x1 (conf scale 1 - face)
        // Output[5]: 400x1  (conf scale 2 - face)
        // Output[6]: 6400x4 (bbox scale 0: [dx, dy, log(w), log(h)])
        // Output[7]: 1600x4 (bbox scale 1: [dx, dy, log(w), log(h)])
        // Output[8]: 400x4  (bbox scale 2: [dx, dy, log(w), log(h)])
        // Output[9]: 6400x10 (landmarks scale 0: 5 keypoints x 2)
        // Output[10]: 1600x10 (landmarks scale 1: 5 keypoints x 2)
        // Output[11]: 400x10  (landmarks scale 2: 5 keypoints x 2)
        
        // Map scales to priors indices
        // YuNet uses 4 feature map scales with steps [8, 16, 32, 64]
        // Multi-scale model uses 3 scales: 80x80 (step 8), 40x40 (step 16), 20x20 (step 32)
        // We need to map each scale to the corresponding priors
        
        struct ScaleInfo {
            int grid_h;
            int grid_w;
            int num_anchors;
            int conf_bg_idx;  // background confidence output index
            int conf_face_idx; // face confidence output index
            int bbox_idx;
            int landmarks_idx;
            int step;  // Step size for this scale (8, 16, or 32)
            int prior_start_idx;  // Starting index in priors array for this scale
            int prior_count;  // Number of priors for this scale
        };
        
        // Define scales with min_sizes matching YuNet's feature map scales
        // Scale 0: 80x80 grid, step 8, min_sizes [10, 16, 24] (maps to YuNet's 3rd feature map)
        // Scale 1: 40x40 grid, step 16, min_sizes [32, 48] (maps to YuNet's 4th feature map)
        // Scale 2: 20x20 grid, step 32, min_sizes [64, 96] (maps to YuNet's 5th feature map)
        std::vector<ScaleInfo> scales;
        scales.push_back({80, 80, 6400, 0, 3, 6, 9, 8, 0, 0});
        scales.push_back({40, 40, 1600, 1, 4, 7, 10, 16, 0, 0});
        scales.push_back({20, 20, 400, 2, 5, 8, 11, 32, 0, 0});
        
        // Calculate prior offsets for each scale
        // Priors are generated for all scales, we need to find the right subset for each scale
        // YuNet generates priors in order: step 8 (3rd feature map), step 16 (4th), step 32 (5th), step 64 (6th)
        // Multi-scale model uses: step 8 (6400 priors), step 16 (1600 priors), step 32 (400 priors)
        int prior_offset_scale0 = 0;  // Priors for step 8 start from index 0
        int prior_offset_scale1 = 0;  // Will be calculated
        int prior_offset_scale2 = 0;  // Will be calculated
        
        // Count priors for each step to calculate offsets
        // Priors are generated in generate_priors() with steps [8, 16, 32, 64]
        // For 640x640 input: step 8 -> 80x80 grid -> 6400 priors (with 3 min_sizes)
        //                    step 16 -> 40x40 grid -> 1600 priors (with 2 min_sizes)
        //                    step 32 -> 20x20 grid -> 400 priors (with 2 min_sizes)
        if (!priors.empty()) {
            // Estimate offsets based on expected prior counts for 640x640 input
            // Scale 0 (step 8): 80x80 grid x 3 min_sizes = 19200, but model has 6400 -> likely 1 min_size per anchor
            // Scale 1 (step 16): 40x40 grid x 2 min_sizes = 3200, but model has 1600 -> likely 1 min_size per anchor
            // Scale 2 (step 32): 20x20 grid x 2 min_sizes = 800, but model has 400 -> likely 1 min_size per anchor
            
            // For now, use simple heuristic: priors are in order by step
            // Try to identify step boundaries by checking prior positions
            int count_step8 = 0, count_step16 = 0, count_step32 = 0;
            for (size_t p = 0; p < priors.size() && p < 1000; ++p) {  // Sample first 1000 priors
                float prior_cx = priors[p].x * padded_w;
                float prior_cy = priors[p].y * padded_h;
                float prior_w = priors[p].width * padded_w;
                
                // Estimate step from prior center position (should be on grid)
                float step_estimate = 0.0f;
                if (prior_cx > 0.001f) {
                    // Find nearest grid position
                    int grid_x = static_cast<int>(std::round(prior_cx / 8.0f));
                    float expected_x = grid_x * 8.0f;
                    if (std::fabs(prior_cx - expected_x) < 2.0f) {
                        step_estimate = 8.0f;
                    } else {
                        grid_x = static_cast<int>(std::round(prior_cx / 16.0f));
                        expected_x = grid_x * 16.0f;
                        if (std::fabs(prior_cx - expected_x) < 2.0f) {
                            step_estimate = 16.0f;
                        } else {
                            step_estimate = 32.0f;
                        }
                    }
                }
                
                if (step_estimate <= 8.1f) count_step8++;
                else if (step_estimate <= 16.1f) count_step16++;
                else count_step32++;
            }
            
            // Extrapolate to full prior set
            if (count_step8 > 0) {
                float ratio = static_cast<float>(priors.size()) / static_cast<float>(count_step8 + count_step16 + count_step32);
                prior_offset_scale1 = static_cast<int>(count_step8 * ratio);
                prior_offset_scale2 = static_cast<int>((count_step8 + count_step16) * ratio);
            } else {
                // Fallback: use expected counts for 640x640 input
                prior_offset_scale1 = 6400;  // Scale 0 has 6400 anchors
                prior_offset_scale2 = 6400 + 1600;  // Scale 0 + Scale 1
            }
            
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Prior offsets: scale0=%d, scale1=%d, scale2=%d (total priors=%zu, sampled: step8=%d, step16=%d, step32=%d)",
                                                     node_name.c_str(), prior_offset_scale0, prior_offset_scale1, prior_offset_scale2, 
                                                     priors.size(), count_step8, count_step16, count_step32));
        }
        
        // Min sizes for each scale (matching YuNet's min_sizes)
        const std::vector<std::vector<float>> min_sizes = {
            {10.0f, 16.0f, 24.0f},  // Scale 0
            {32.0f, 48.0f},         // Scale 1
            {64.0f, 96.0f}          // Scale 2
        };
        
        int total_candidates = 0;
        float max_score_found = 0.0f;
        float max_conf_val0 = -1e6f;
        float max_conf_val1 = -1e6f;
        float min_conf_val0 = 1e6f;
        float min_conf_val1 = 1e6f;
        float sum_conf_val0 = 0.0f;
        float sum_conf_val1 = 0.0f;
        int conf_sample_count = 0;
        
        // Debug: log first few values to understand format
        bool logged_sample_values = false;
        int sample_count = 0;
        
        // Process each scale
        for (const auto& scale : scales) {
            if (scale.conf_face_idx >= static_cast<int>(raw_outputs.size()) ||
                scale.bbox_idx >= static_cast<int>(raw_outputs.size()) ||
                scale.landmarks_idx >= static_cast<int>(raw_outputs.size())) {
                continue;
            }
            
            const cv::Mat& conf_bg = raw_outputs[scale.conf_bg_idx];
            const cv::Mat& conf_face = raw_outputs[scale.conf_face_idx];
            const cv::Mat& bbox = raw_outputs[scale.bbox_idx];
            const cv::Mat& landmarks = raw_outputs[scale.landmarks_idx];
            
            if (conf_face.rows != scale.num_anchors || bbox.rows != scale.num_anchors ||
                landmarks.rows != scale.num_anchors) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Scale mismatch: conf=%d bbox=%d landmarks=%d expected=%d",
                                                        node_name.c_str(),
                                                        conf_face.rows, bbox.rows, landmarks.rows, scale.num_anchors));
                continue;
            }
            
            const float* conf_bg_data = conf_bg.ptr<float>();
            const float* conf_face_data = conf_face.ptr<float>();
            const float* bbox_data = bbox.ptr<float>();
            const float* landmarks_data = landmarks.ptr<float>();
            
            // Process each anchor in this scale
            for (int anchor_idx = 0; anchor_idx < scale.num_anchors; ++anchor_idx) {
                float conf_val0 = conf_bg_data[anchor_idx];
                float conf_val1 = conf_face_data[anchor_idx];
                
                // Track statistics
                if (conf_val0 > max_conf_val0) max_conf_val0 = conf_val0;
                if (conf_val1 > max_conf_val1) max_conf_val1 = conf_val1;
                if (conf_val0 < min_conf_val0) min_conf_val0 = conf_val0;
                if (conf_val1 < min_conf_val1) min_conf_val1 = conf_val1;
                
                // Sample first few values for debugging
                if (!logged_sample_values && sample_count < 10) {
                    sum_conf_val0 += conf_val0;
                    sum_conf_val1 += conf_val1;
                    conf_sample_count++;
                    sample_count++;
                    
                    if (sample_count >= 10) {
                        float avg_conf0 = sum_conf_val0 / conf_sample_count;
                        float avg_conf1 = sum_conf_val1 / conf_sample_count;
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Sample conf values (first 10): avg_bg=%.4f, avg_face=%.4f, sum=%.4f",
                                                                 node_name.c_str(), avg_conf0, avg_conf1, avg_conf0 + avg_conf1));
                        logged_sample_values = true;
                    }
                }
                
                // Calculate score based on output format
                // Multi-scale YuNet typically outputs sigmoid probabilities or softmax probabilities
                float score = 0.0f;
                
                // Check if outputs are logits (can be negative or > 1) or probabilities (0-1)
                bool is_logit = (conf_val0 < 0 || conf_val1 < 0) || 
                                (std::fabs(conf_val0) > 5.0f || std::fabs(conf_val1) > 5.0f);
                
                if (is_logit) {
                    // Apply sigmoid to logits (common in RKNN quantized models)
                    // sigmoid(x) = 1 / (1 + exp(-x))
                    float prob0 = 1.0f / (1.0f + std::exp(-conf_val0));
                    float prob1 = 1.0f / (1.0f + std::exp(-conf_val1));
                    // Normalize to get probability distribution
                    float sum_prob = prob0 + prob1;
                    if (sum_prob > 1e-6f) {
                        score = prob1 / sum_prob;
                    } else {
                        score = prob1;  // Fallback to direct probability
                    }
                } else {
                    // Already probabilities (0-1 range)
                    // If they sum to ~1, they're normalized probabilities
                    float sum_prob = conf_val0 + conf_val1;
                    if (std::fabs(sum_prob - 1.0f) < 0.1f) {
                        // Normalized probabilities
                        score = conf_val1;
                    } else if (sum_prob > 1e-6f) {
                        // Not normalized, normalize them
                        score = conf_val1 / sum_prob;
                    } else {
                        // Use face probability directly
                        score = conf_val1;
                    }
                }
                
                // Clamp score to [0, 1]
                score = std::max(0.0f, std::min(1.0f, score));
                
                if (score > max_score_found) {
                    max_score_found = score;
                }
                
                if (score < score_threshold) {
                    continue;
                }
                
                total_candidates++;
                
                // Decode bbox using YuNet format: [dx, dy, log(w), log(h)]
                // Calculate grid position for this anchor
                int grid_x = anchor_idx % scale.grid_w;
                int grid_y = anchor_idx / scale.grid_w;
                
                // Get bbox values (offsets and log scales)
                const float* bbox_ptr = bbox_data + anchor_idx * 4;
                float dx = bbox_ptr[0];
                float dy = bbox_ptr[1];
                float log_w = bbox_ptr[2];
                float log_h = bbox_ptr[3];
                
                // Find corresponding prior for this anchor using grid position
                // Calculate expected prior center from grid position
                float expected_cx_norm = (grid_x + 0.5f) * scale.step / padded_w;
                float expected_cy_norm = (grid_y + 0.5f) * scale.step / padded_h;
                
                // Find the closest prior that matches this grid position
                size_t prior_idx = 0;
                float min_distance = 1e6f;
                
                // Search through priors to find the best match
                // Limit search to reasonable range to avoid performance issues
                size_t search_start = 0;
                size_t search_end = priors.size();
                
                // Optimize: priors are generated in order by scale, so we can narrow the search
                if (scale.step == 8 && prior_offset_scale1 > 0) {
                    search_end = std::min(static_cast<size_t>(prior_offset_scale1), priors.size());
                } else if (scale.step == 16 && prior_offset_scale2 > prior_offset_scale1) {
                    search_start = prior_offset_scale1;
                    search_end = std::min(static_cast<size_t>(prior_offset_scale2), priors.size());
                } else if (scale.step == 32) {
                    search_start = prior_offset_scale2;
                }
                
                for (size_t p = search_start; p < search_end; ++p) {
                    float prior_dx = std::fabs(priors[p].x - expected_cx_norm);
                    float prior_dy = std::fabs(priors[p].y - expected_cy_norm);
                    float distance = prior_dx + prior_dy;  // Manhattan distance
                    
                    if (distance < min_distance) {
                        min_distance = distance;
                        prior_idx = p;
                    }
                    
                    // Early exit if we found a very close match
                    if (distance < 0.01f) {
                        break;
                    }
                }
                
                // Fallback: if no good match found, use simple index mapping
                if (min_distance > 0.1f && scale.num_anchors > 0) {
                    // Use linear mapping as fallback
                    int prior_offset = (scale.step == 8) ? prior_offset_scale0 :
                                      (scale.step == 16) ? prior_offset_scale1 : prior_offset_scale2;
                    float ratio = static_cast<float>(anchor_idx) / static_cast<float>(scale.num_anchors);
                    int mapped_idx = static_cast<int>(ratio * (search_end - search_start));
                    prior_idx = search_start + mapped_idx;
                    if (prior_idx >= priors.size()) {
                        prior_idx = priors.size() - 1;
                    }
                }
                
                // Decode bbox using YuNet formula with the matched prior
                const cv::Rect2f& prior = priors[prior_idx];
                float cx = (prior.x + dx * variance[0] * prior.width) * padded_w;
                float cy = (prior.y + dy * variance[0] * prior.height) * padded_h;
                float w = prior.width * std::exp(log_w * variance[0]) * padded_w;
                float h = prior.height * std::exp(log_h * variance[1]) * padded_h;
                float x1 = cx - w / 2.0f;
                float y1 = cy - h / 2.0f;
                
                // Debug log for first few detections
                if (total_candidates <= 3) {
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Detection #%d: grid=(%d,%d), bbox_raw=[dx=%.4f, dy=%.4f, log_w=%.4f, log_h=%.4f] -> x1=%.1f, y1=%.1f, w=%.1f, h=%.1f",
                                                             node_name.c_str(), total_candidates, grid_x, grid_y, dx, dy, log_w, log_h, x1, y1, w, h));
                }
                
                // Filter out invalid boxes
                if (w < 10.0f || h < 10.0f || w > padded_w || h > padded_h) {
                    continue;  // Skip boxes that are too small or too large
                }
                
                // Filter by aspect ratio (faces should be roughly square to slightly rectangular)
                float aspect_ratio = w / h;
                if (aspect_ratio < 0.3f || aspect_ratio > 3.0f) {
                    continue;  // Skip boxes with extreme aspect ratios
                }
                
                // Decode landmarks using YuNet format with the matched prior
                // Landmarks format: [dx_re, dy_re, dx_le, dy_le, dx_nt, dy_nt, dx_rcm, dy_rcm, dx_lcm, dy_lcm]
                // Order: right eye, left eye, nose tip, right corner of mouth, left corner of mouth
                std::vector<std::pair<int, int>> kps(5);
                const float* kp_ptr = landmarks_data + anchor_idx * 10;
                // Note: 'prior' is already declared above at line 856
                
                for (int kp_idx = 0; kp_idx < 5; ++kp_idx) {
                    float kp_dx = kp_ptr[kp_idx * 2 + 0];
                    float kp_dy = kp_ptr[kp_idx * 2 + 1];
                    
                    // Decode landmark using YuNet formula: landmark = prior_center + offset * variance * prior_size
                    float kp_x = (prior.x + kp_dx * variance[0] * prior.width) * padded_w;
                    float kp_y = (prior.y + kp_dy * variance[0] * prior.height) * padded_h;
                    
                    // Clamp to padded bounds before removing padding
                    kp_x = std::max(0.0f, std::min(kp_x, padded_w - 1.0f));
                    kp_y = std::max(0.0f, std::min(kp_y, padded_h - 1.0f));
                    
                    kps[kp_idx] = std::make_pair(static_cast<int>(kp_x), static_cast<int>(kp_y));
                }
                
                // Remove padding
                x1 = x1 - pad_left_f;
                y1 = y1 - pad_top_f;
                for (auto& kp : kps) {
                    kp.first = static_cast<int>(kp.first - pad_left_f);
                    kp.second = static_cast<int>(kp.second - pad_top_f);
                }
                
                // Scale back to original frame size
                if (inferred_scale > 1e-6f) {
                    x1 = x1 / inferred_scale;
                    y1 = y1 / inferred_scale;
                    w = w / inferred_scale;
                    h = h / inferred_scale;
                    for (auto& kp : kps) {
                        kp.first = static_cast<int>(kp.first / inferred_scale);
                        kp.second = static_cast<int>(kp.second / inferred_scale);
                    }
                }
                
                // Convert to integer coordinates
                int x = static_cast<int>(std::round(x1));
                int y = static_cast<int>(std::round(y1));
                int width_int = static_cast<int>(std::round(w));
                int height_int = static_cast<int>(std::round(h));
                
                // Clamp to frame bounds
                x = std::max(0, std::min(x, static_cast<int>(frame_width_f) - 1));
                y = std::max(0, std::min(y, static_cast<int>(frame_height_f) - 1));
                width_int = std::max(10, std::min(width_int, static_cast<int>(frame_width_f) - x));  // Min 10 pixels
                height_int = std::max(10, std::min(height_int, static_cast<int>(frame_height_f) - y)); // Min 10 pixels
                
                // Final validation
                if (width_int < 10 || height_int < 10) {
                    continue;
                }
                
                boxes.emplace_back(x, y, width_int, height_int);
                scores.push_back(score);
                keypoints.push_back(kps);
            }
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("[%s] parse_multiscale_outputs: Found %zu detections above score_threshold=%.2f (from 3 scales, %d candidates passed threshold)",
                                                  node_name.c_str(),
                                                  boxes.size(),
                                                  score_threshold,
                                                  total_candidates));
        if (boxes.empty()) {
            // Calculate average conf values for better diagnostics
            float avg_conf0 = (min_conf_val0 < 1e6f) ? (min_conf_val0 + max_conf_val0) / 2.0f : 0.0f;
            float avg_conf1 = (min_conf_val1 < 1e6f) ? (min_conf_val1 + max_conf_val1) / 2.0f : 0.0f;
            float conf_sum_avg = avg_conf0 + avg_conf1;
            
            CVEDIX_INFO(cvedix_utils::string_format("[%s] No detections above threshold. Max score found: %.4f (threshold=%.2f).", 
                                                     node_name.c_str(), max_score_found, score_threshold));
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Conf stats - bg: [%.4f, %.4f] avg=%.4f, face: [%.4f, %.4f] avg=%.4f, sum_avg=%.4f",
                                                     node_name.c_str(),
                                                     min_conf_val0, max_conf_val0, avg_conf0,
                                                     min_conf_val1, max_conf_val1, avg_conf1, conf_sum_avg));
            if (max_score_found > 0.0f && max_score_found < score_threshold) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Consider lowering threshold from %.2f to %.2f to capture detections",
                                                         node_name.c_str(), score_threshold, max_score_found * 0.9f));
            }
        }
    }

    void cvedix_rknn_face_detector_node::parse_yunet_outputs(
        const cv::Mat& loc_mat,
        const cv::Mat& conf_mat,
        const cv::Mat& iou_mat,
        const letterbox_params& lb_params,
        const cv::Size& frame_size,
        std::vector<cv::Rect>& boxes,
        std::vector<float>& scores,
        std::vector<std::vector<std::pair<int, int>>>& keypoints) {
        
        using namespace cv;
        
        boxes.clear();
        scores.clear();
        keypoints.clear();
        
        // Ensure priors are generated for current input size
        if (priors.empty() || current_input_w != lb_params.padded_width || current_input_h != lb_params.padded_height) {
            generate_priors(lb_params.padded_width, lb_params.padded_height);
        }
        
        if (loc_mat.rows != static_cast<int>(priors.size()) || 
            conf_mat.rows != static_cast<int>(priors.size()) ||
            iou_mat.rows != static_cast<int>(priors.size())) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Output size mismatch: loc=%d conf=%d iou=%d priors=%zu",
                                                    node_name.c_str(),
                                                    loc_mat.rows, conf_mat.rows, iou_mat.rows, priors.size()));
            return;
        }
        
        // YuNet variance parameters (matching OpenCV FaceDetectorYN implementation)
        const std::vector<float> variance = {0.1f, 0.2f};
        const float* loc_v = (const float*)(loc_mat.data);
        const float* conf_v = (const float*)(conf_mat.data);
        const float* iou_v = (const float*)(iou_mat.data);
        
        // Pre-calculate transformation parameters for efficiency
        const float padded_w = static_cast<float>(lb_params.padded_width);
        const float padded_h = static_cast<float>(lb_params.padded_height);
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
        
        // First pass: Calculate IOU statistics to determine normalization method
        float raw_max_iou = -1e6f;
        float raw_min_iou = 1e6f;
        for (size_t i = 0; i < priors.size(); ++i) {
            float iou_raw = iou_v[i];
            if (iou_raw > raw_max_iou) raw_max_iou = iou_raw;
            if (iou_raw < raw_min_iou) raw_min_iou = iou_raw;
        }
        
        // Determine IOU normalization method based on value range
        bool use_scale_normalization = (raw_max_iou > 1.5f);  // If max > 1.5, likely confidence scores
        float iou_scale_factor = 1.0f;
        if (use_scale_normalization) {
            // Scale IOU values: map [0, max] to [0, 1] or use sigmoid
            // Try mapping [0, 2.0] to [0, 1.0] for values up to 2.0, then saturate
            iou_scale_factor = 2.0f;  // Divide by 2.0 to normalize
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Using scale normalization for IOU: raw_range=[%.4f, %.4f], scale_factor=%.2f",
                                                     node_name.c_str(), raw_min_iou, raw_max_iou, iou_scale_factor));
        } else {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Using clamp normalization for IOU: raw_range=[%.4f, %.4f]",
                                                     node_name.c_str(), raw_min_iou, raw_max_iou));
        }
        
        // Statistics for debugging
        int candidates_above_threshold = 0;
        float max_score_found = 0.0f;
        float max_cls_score = 0.0f;
        float max_iou_score = 0.0f;
        float min_cls_score = 1e6f;
        float min_iou_score = 1e6f;
        
        // Track top candidates for detailed debugging
        struct TopCandidate {
            size_t idx;
            float cls_score;
            float iou_score_raw;
            float iou_score_processed;
            float combined_score;
        };
        std::vector<TopCandidate> top_candidates;
        top_candidates.reserve(10);
        
        // Process each prior - matching OpenCV FaceDetectorYN format exactly
        // Output format: [x1, y1, w, h, x_re, y_re, x_le, y_le, x_nt, y_nt, x_rcm, y_rcm, x_lcm, y_lcm, conf]
        for (size_t i = 0; i < priors.size(); ++i) {
            // Get confidence scores - matching YuNet format
            float clsScore = conf_v[i*2+1];  // Face class score (index 1 in 2-class output)
            float iouScoreRaw = iou_v[i];  // Raw IOU score before processing
            
            // Process IOU score based on determined method
            float iouScore = iouScoreRaw;
            if (use_scale_normalization) {
                // Scale normalization: map [0, scale_factor] to [0, 1]
                if (iouScoreRaw < 0.f) {
                    iouScore = 0.f;
                } else if (iouScoreRaw > iou_scale_factor) {
                    iouScore = 1.0f;  // Saturate at 1.0
                } else {
                    iouScore = iouScoreRaw / iou_scale_factor;
                }
            } else {
                // Standard YuNet clamping: clamp to [0, 1]
                if (iouScore < 0.f) {
                    iouScore = 0.f;
                } else if (iouScore > 1.f) {
                    iouScore = 1.f;
                }
            }
            
            // Combined score: sqrt(clsScore * iouScore) - matching OpenCV FaceDetectorYN
            float score = std::sqrt(clsScore * iouScore);
            
            // Track statistics
            if (clsScore > max_cls_score) max_cls_score = clsScore;
            if (clsScore < min_cls_score) min_cls_score = clsScore;
            if (iouScore > max_iou_score) max_iou_score = iouScore;
            if (iouScore < min_iou_score) min_iou_score = iouScore;
            if (score > max_score_found) max_score_found = score;
            
            // Track top candidates for debugging
            if (top_candidates.size() < 10 || score > top_candidates.back().combined_score) {
                TopCandidate candidate;
                candidate.idx = i;
                candidate.cls_score = clsScore;
                candidate.iou_score_raw = iouScoreRaw;
                candidate.iou_score_processed = iouScore;
                candidate.combined_score = score;
                top_candidates.push_back(candidate);
                
                // Keep only top 10
                if (top_candidates.size() > 10) {
                    std::sort(top_candidates.begin(), top_candidates.end(), 
                             [](const TopCandidate& a, const TopCandidate& b) {
                                 return a.combined_score > b.combined_score;
                             });
                    top_candidates.resize(10);
                }
            }
            
            // Filter by score threshold
            if (score < score_threshold) {
                continue;
            }
            
            candidates_above_threshold++;
            
            // Decode bounding box from deltas and priors - matching YuNet exactly
            // loc format: [dx, dy, log(w), log(h), ...landmarks...]
            const float* loc_ptr = loc_v + i * 14;
            const float prior_x = priors[i].x;
            const float prior_y = priors[i].y;
            const float prior_w = priors[i].width;
            const float prior_h = priors[i].height;
            
            // Bounding box center and size in padded input space
            float cx = (prior_x + loc_ptr[0] * variance[0] * prior_w) * padded_w;
            float cy = (prior_y + loc_ptr[1] * variance[0] * prior_h) * padded_h;
            float w  = prior_w * std::exp(loc_ptr[2] * variance[0]) * padded_w;
            float h  = prior_h * std::exp(loc_ptr[3] * variance[1]) * padded_h;
            float x1 = cx - w / 2.0f;
            float y1 = cy - h / 2.0f;
            
            // Decode landmarks (5 keypoints) - matching YuNet format exactly
            // Order: right eye, left eye, nose tip, right corner of mouth, left corner of mouth
            std::vector<std::pair<int, int>> kps(5);
            for (int kp_idx = 0; kp_idx < 5; ++kp_idx) {
                float kp_x = (prior_x + loc_ptr[4 + kp_idx*2] * variance[0] * prior_w) * padded_w;
                float kp_y = (prior_y + loc_ptr[5 + kp_idx*2] * variance[0] * prior_h) * padded_h;
                kps[kp_idx] = std::make_pair(static_cast<int>(kp_x), static_cast<int>(kp_y));
            }
            
            // Transform from padded input space to original frame space
            // Step 1: Remove padding
            x1 = x1 - pad_left_f;
            y1 = y1 - pad_top_f;
            for (auto& kp : kps) {
                kp.first = static_cast<int>(kp.first - pad_left_f);
                kp.second = static_cast<int>(kp.second - pad_top_f);
            }
            
            // Step 2: Scale back to original frame size
            if (inferred_scale > 1e-6f) {
                x1 = x1 * inv_scale;
                y1 = y1 * inv_scale;
                w = w * inv_scale;
                h = h * inv_scale;
                for (auto& kp : kps) {
                    kp.first = static_cast<int>(kp.first * inv_scale);
                    kp.second = static_cast<int>(kp.second * inv_scale);
                }
            }
            
            // Convert to integer coordinates
            int x = static_cast<int>(std::round(x1));
            int y = static_cast<int>(std::round(y1));
            int width = static_cast<int>(std::round(w));
            int height = static_cast<int>(std::round(h));
            
            // Clamp to frame bounds (matching OpenCV FaceDetectorYN behavior)
            x = std::max(0, std::min(x, static_cast<int>(frame_width_f) - 1));
            y = std::max(0, std::min(y, static_cast<int>(frame_height_f) - 1));
            width = std::max(1, std::min(width, static_cast<int>(frame_width_f) - x));
            height = std::max(1, std::min(height, static_cast<int>(frame_height_f) - y));
            
            // Clamp keypoints to frame bounds
            for (auto& kp : kps) {
                kp.first = std::max(0, std::min(kp.first, static_cast<int>(frame_width_f) - 1));
                kp.second = std::max(0, std::min(kp.second, static_cast<int>(frame_height_f) - 1));
            }
            
            boxes.emplace_back(x, y, width, height);
            scores.push_back(score);
            keypoints.push_back(kps);
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("[%s] parse_yunet_outputs: Found %zu detections above score_threshold=%.2f (from %zu priors, %d candidates passed threshold)",
                                                  node_name.c_str(),
                                                  boxes.size(),
                                                  score_threshold,
                                                  priors.size(),
                                                  candidates_above_threshold));
        if (boxes.empty() && priors.size() > 0) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] No detections above threshold. Max scores: cls=%.4f, iou=%.4f (raw: [%.4f, %.4f]), combined=%.4f (threshold=%.2f)",
                                                     node_name.c_str(),
                                                     max_cls_score,
                                                     max_iou_score,
                                                     raw_min_iou, raw_max_iou,
                                                     max_score_found,
                                                     score_threshold));
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Score ranges: cls=[%.4f, %.4f], iou=[%.4f, %.4f] (clamped), iou_raw=[%.4f, %.4f]",
                                                     node_name.c_str(),
                                                     min_cls_score, max_cls_score,
                                                     min_iou_score, max_iou_score,
                                                     raw_min_iou, raw_max_iou));
            
            // Log top candidates for detailed analysis
            std::sort(top_candidates.begin(), top_candidates.end(), 
                     [](const TopCandidate& a, const TopCandidate& b) {
                         return a.combined_score > b.combined_score;
                     });
            
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Top 5 candidates:", node_name.c_str()));
            for (size_t j = 0; j < std::min(5UL, top_candidates.size()); ++j) {
                const auto& c = top_candidates[j];
                CVEDIX_INFO(cvedix_utils::string_format("[%s]   #%zu: idx=%zu, cls=%.4f, iou_raw=%.4f, iou_processed=%.4f, combined=%.4f",
                                                         node_name.c_str(), j+1, c.idx, c.cls_score, 
                                                         c.iou_score_raw, c.iou_score_processed, c.combined_score));
            }
            
            if (max_score_found > 0.0f && max_score_found < score_threshold) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Consider lowering threshold from %.2f to %.2f to capture detections",
                                                         node_name.c_str(), score_threshold, max_score_found * 0.9f));
            }
        }
    }

    void cvedix_rknn_face_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Empty outputs or frame meta", node_name.c_str()));
            return;
        }

        auto& frame_meta = frame_meta_with_batch[0];
        cv::Mat& frame = frame_meta->frame;

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
        std::vector<std::vector<std::pair<int, int>>> keypoints;
        
        // Detect output format: 3 outputs (YuNet) or 12 outputs (multi-scale)
        // YuNet format matches OpenCV FaceDetectorYN:
        // - loc: [num_priors, 14] - bounding box (4) + landmarks (10)
        // - conf: [num_priors, 2] - background and face class scores
        // - iou: [num_priors, 1] - intersection over union scores
        // Final output format: [x1, y1, w, h, x_re, y_re, x_le, y_le, x_nt, y_nt, x_rcm, y_rcm, x_lcm, y_lcm, conf]
        if (raw_outputs.size() == 3) {
            // Standard YuNet format: loc, conf, iou
            const cv::Mat& loc_mat = raw_outputs[0];
            const cv::Mat& conf_mat = raw_outputs[1];
            const cv::Mat& iou_mat = raw_outputs[2];
            parse_yunet_outputs(loc_mat, conf_mat, iou_mat, lb_params, frame.size(), 
                             boxes, scores, keypoints);
        } else if (raw_outputs.size() == 12) {
            // Multi-scale format: 3 scales x 4 outputs per scale
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Detected multi-scale format (12 outputs)", node_name.c_str()));
            parse_multiscale_outputs(raw_outputs, lb_params, frame.size(), 
                                    boxes, scores, keypoints);
        } else {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Unsupported output format: expected 3 or 12 outputs but got %zu", 
                                                    node_name.c_str(), raw_outputs.size()));
            return;
        }

        CVEDIX_INFO(cvedix_utils::string_format("[%s] Parsed %zu detections before NMS (frame %llu, channel %d)",
                                                node_name.c_str(),
                                                boxes.size(),
                                                frame_meta->frame_index,
                                                frame_meta->channel_index));

        if (boxes.empty()) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] No detections above threshold (score_threshold=%.2f, frame %llu, channel %d)",
                                                    node_name.c_str(),
                                                    score_threshold,
                                                    frame_meta->frame_index,
                                                    frame_meta->channel_index));
            return;
        }

        // Apply NMS - matching OpenCV FaceDetectorYN behavior
        // Note: OpenCV FaceDetectorYN uses score_threshold for filtering before NMS,
        // and nms_threshold for IoU threshold during NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, scores, score_threshold, nms_threshold, indices, 1.0f, top_k);
        
        CVEDIX_INFO(cvedix_utils::string_format("[%s] After NMS: %zu faces detected (from %zu candidates, frame %llu, channel %d)",
                                                node_name.c_str(),
                                                indices.size(),
                                                boxes.size(),
                                                frame_meta->frame_index,
                                                frame_meta->channel_index));

        // Create face targets
        for (int idx : indices) {
            const cv::Rect& box = boxes[idx];
            const float score = scores[idx];
            const std::vector<std::pair<int, int>>& kps = keypoints[idx];

            // Clamp keypoints to frame bounds
            std::vector<std::pair<int, int>> clamped_kps = kps;
            for (auto& kp : clamped_kps) {
                kp.first = std::max(0, std::min(kp.first, frame.cols - 1));
                kp.second = std::max(0, std::min(kp.second, frame.rows - 1));
            }

            auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                box.x,
                box.y,
                box.width,
                box.height,
                score,
                clamped_kps);

            frame_meta->face_targets.push_back(face_target);
            
            // Also add to regular targets for compatibility
            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                box.x,
                box.y,
                box.width,
                box.height,
                0,  // class_id for face
                score,
                frame_meta->frame_index,
                frame_meta->channel_index,
                "Face");

            frame_meta->targets.push_back(target);

            // Log chi tiết từng bounding box được phát hiện
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [Face #%zu] Bounding Box: x=%d, y=%d, width=%d, height=%d | Score=%.4f | Keypoints=%zu | Frame=%llu, Channel=%d",
                node_name.c_str(),
                frame_meta->face_targets.size(),
                box.x,
                box.y,
                box.width,
                box.height,
                score,
                clamped_kps.size(),
                frame_meta->frame_index,
                frame_meta->channel_index));
        }

        CVEDIX_INFO(cvedix_utils::string_format("[%s] ===== Total detected: %zu faces (Frame %llu, Channel %d) =====",
                                                node_name.c_str(),
                                                frame_meta->face_targets.size(),
                                                frame_meta->frame_index,
                                                frame_meta->channel_index));
    }

} // namespace cvedix_nodes

