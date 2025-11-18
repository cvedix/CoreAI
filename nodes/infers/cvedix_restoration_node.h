#pragma once

#include "base/cvedix_primary_infer_node.h"

namespace cvedix_nodes {
    // general image restoration node using Real-ESRGAN
    // used to enhance quality of frames in video， see more: https://github.com/xinntao/Real-ESRGAN
    class cvedix_restoration_node: public cvedix_primary_infer_node
    {   
    private:
        /* onnx network using opencv::dnn as backend */
        cv::dnn::Net restoration_net;
        bool restoration_to_osd = true;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_restoration_node(std::string node_name, 
                            std::string realesrgan_bg_restoration_model,
                            std::string face_restoration_model = "",
                            bool restoration_to_osd = true);
        ~cvedix_restoration_node();
    };
}