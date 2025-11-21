
#include "cvedix_restoration_node.h"

namespace cvedix_nodes {
    
    cvedix_restoration_node::cvedix_restoration_node(std::string node_name, 
                            std::string realesrgan_bg_restoration_model,
                            std::string face_restoration_model,
                            bool restoration_to_osd):
                            cvedix_primary_infer_node(node_name, ""),
                            restoration_to_osd(restoration_to_osd) {        
        /* init net*/
        restoration_net = cv::dnn::readNetFromONNX(realesrgan_bg_restoration_model);
        /* to-do, load face restoration model*/
        #ifdef CVEDIX_WITH_CUDA
        //restoration_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        //restoration_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        #endif
        this->initialized();
    }
    
    cvedix_restoration_node::~cvedix_restoration_node() {
        deinitialized();
    }

    // please refer to cvedix_infer_node::run_infer_combinations
    void cvedix_restoration_node::run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        assert(frame_meta_with_batch.size() == 1);
        auto& frame_meta = frame_meta_with_batch[0];

        // 4x larger for RealESRGAN_x4plus model
        cv::Mat target_blob = cv::dnn::blobFromImage(frame_meta->frame, 1 / 255.0, cv::Size(frame_meta->frame.cols, frame_meta->frame.rows), (0, 0, 0), true);
        std::vector<cv::Mat> target_outputs;
        restoration_net.setInput(target_blob);
        restoration_net.forward(target_outputs, restoration_net.getUnconnectedOutLayersNames());

        // parse to image
        auto& output = target_outputs[0];
        cv::Mat output_channel_last;
        // Helper function to transpose NCHW -> NHWC (replacement for cv::transposeND in OpenCV < 4.8)
        if (output.dims == 4) {
            int n = output.size[0], c = output.size[1], h = output.size[2], w = output.size[3];
            std::vector<int> new_shape = {n, h, w, c};
            output_channel_last = cv::Mat(4, new_shape.data(), output.type());
            const float* src_data = (const float*)output.data;
            float* dst_data = (float*)output_channel_last.data;
            for (int ni = 0; ni < n; ni++) {
                for (int hi = 0; hi < h; hi++) {
                    for (int wi = 0; wi < w; wi++) {
                        for (int ci = 0; ci < c; ci++) {
                            int src_idx = ni * (c * h * w) + ci * (h * w) + hi * w + wi;
                            int dst_idx = ni * (h * w * c) + hi * (w * c) + wi * c + ci;
                            dst_data[dst_idx] = src_data[src_idx];
                        }
                    }
                }
            }
        } else {
            output_channel_last = output.clone();
        }
        cv::Mat img_result(output_channel_last.size[1], output_channel_last.size[2], CV_32FC3, output_channel_last.data);
        img_result.convertTo(img_result, CV_8U, 255);
        
        // update back to frame meta
        auto& bg = restoration_to_osd ? frame_meta->osd_frame : frame_meta->frame;
        cv::cvtColor(img_result, bg, cv::COLOR_RGB2BGR);
    }

    void cvedix_restoration_node::postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    }
}