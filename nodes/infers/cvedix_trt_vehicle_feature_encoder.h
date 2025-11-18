#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_secondary_infer_node.h"
#include "../../third_party/trt_vehicle/models/vehicle_feature_encoder.h"

namespace cvedix_nodes {
    // vehicle feature encoder based on tensorrt using trt_vehicle library
    // update embeddings of cvedix_frame_target
    class cvedix_trt_vehicle_feature_encoder: public cvedix_secondary_infer_node
    {
    private:
        /* data */
        std::shared_ptr<trt_vehicle::VehicleFeatureEncoder> vehicle_feature_encoder = nullptr;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_trt_vehicle_feature_encoder(std::string node_name, std::string vehicle_feature_model_path = "", std::vector<int> p_class_ids_applied_to = std::vector<int>(), int min_width_applied_to = 0, int min_height_applied_to = 0);
        ~cvedix_trt_vehicle_feature_encoder();
    };
}
#endif