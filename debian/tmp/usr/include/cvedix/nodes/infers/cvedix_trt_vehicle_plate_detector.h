#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_secondary_infer_node.h"
#include "cvedix/objects/cvedix_sub_target.h"
#include "../../third_party/trt_vehicle/models/vehicle_plate_detector.h"

namespace cvedix_nodes {
    // vehicle plate detector based on tensorrt using trt_vehicle library
    // source code: ../../third_party/trt_vehicle
    // note: derived from cvedix_secondary_infer_node since it detects plates on small cropped images, which is different from cvedix_trt_vehicle_plate_detector_v2 class
    // this class is not based on opencv::dnn module but tensorrt, a few data members declared in base class are not usable any more(just ignore), such as cvedix_infer_node::net.
    class cvedix_trt_vehicle_plate_detector: public cvedix_secondary_infer_node
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
        cvedix_trt_vehicle_plate_detector(std::string node_name, std::string plate_det_model_path = "", std::string char_rec_model_path = "");
        ~cvedix_trt_vehicle_plate_detector();
    };
}
#endif