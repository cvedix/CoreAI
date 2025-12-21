/**
 * @file cvedix_trt_vehicle_plate_detector_v2.h
 * @brief TensorRT license plate detector (primary - on full frame)
 * 
 * @warning Currently disabled due to TensorRT 10.x API incompatibility
 */

#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_primary_infer_node.h"

namespace trt_vehicle { class VehiclePlateDetector; }

namespace cvedix_nodes {
    /**
     * @brief TensorRT plate detector - primary (currently disabled)
     */
    class cvedix_trt_vehicle_plate_detector_v2: public cvedix_primary_infer_node
    {

    private:
        /* data */
        std::shared_ptr<trt_vehicle::VehiclePlateDetector> plate_detector = nullptr;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_trt_vehicle_plate_detector_v2(std::string node_name, std::string plate_det_model_path = "", std::string char_rec_model_path = "");
        ~cvedix_trt_vehicle_plate_detector_v2();
    };
}
#endif