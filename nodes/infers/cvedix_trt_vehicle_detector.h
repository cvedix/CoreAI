#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_primary_infer_node.h"
// trt_vehicle is currently disabled due to TensorRT 10.x API incompatibility
// #include "third_party/trt_vehicle/models/vehicle_detector.h"

// Forward declaration (trt_vehicle library is disabled)
namespace trt_vehicle {
    class VehicleDetector;
}

namespace cvedix_nodes {
    // vehicle detector based on tensorrt using trt_vehicle library
    // NOTE: trt_vehicle library is currently disabled - this class will not work until trt_vehicle is re-enabled
    class cvedix_trt_vehicle_detector: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<trt_vehicle::VehicleDetector> vehicle_detector = nullptr;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_trt_vehicle_detector(std::string node_name, std::string vehicle_det_model_path = "");
        ~cvedix_trt_vehicle_detector();
    };
}
#endif