#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_primary_infer_node.h"
#include "../../third_party/trt_yolov8/trt_yolov8_detector.h"

namespace cvedix_nodes {
    // universal yolov8 detector based on tensorrt using third_party/trt_yolov8 library
    class cvedix_trt_yolov8_detector: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<trt_yolov8::trt_yolov8_detector> yolov8_detector = nullptr;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_trt_yolov8_detector(std::string node_name, std::string model_path, std::string labels_path = "");
        ~cvedix_trt_yolov8_detector();
    };
}
#endif