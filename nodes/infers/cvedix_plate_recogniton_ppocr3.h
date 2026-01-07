/**
 * @file cvedix_plate_recogniton_ppocr3.h
 * @brief PaddleOCR license plate recognition node
 * 
 * Secondary inference node that runs PaddleOCR on cropped license plates.
 */

#pragma once

#ifdef CVEDIX_WITH_PADDLE
#include "base/cvedix_secondary_infer_node.h"
#include "cvedix/third_party/paddle_ocr/include/paddleocr.h"

namespace cvedix_nodes {
    /**
     * @brief PaddleOCR plate recognizer
     * 
     * Runs OCR on objects detected by a primary detector (e.g. valid plates).
     */
    class cvedix_plate_recogniton_ppocr3: public cvedix_secondary_infer_node
    {

    private:
        // paddle ocr instance
        std::shared_ptr<PaddleOCR::PPOCR> ocr;
        
        // Configuration
        std::string det_model_dir;
        std::string cls_model_dir;
        std::string rec_model_dir;
        std::string rec_char_dict_path;

    protected:
        // logic for the whole infer combinations
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
        // override pure virtual method
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        cvedix_plate_recogniton_ppocr3(std::string node_name, 
                                    std::string det_model_dir, 
                                    std::string cls_model_dir, 
                                    std::string rec_model_dir, 
                                    std::string rec_char_dict_path,
                                    std::vector<int> p_class_ids_applied_to = {0}, // Default apply to class 0
                                    int min_width_applied_to = 0,
                                    int min_height_applied_to = 0,
                                    int crop_padding = 0,
                                    bool use_tensorrt = true,
                                    std::string precision = "fp16");
                                    
        ~cvedix_plate_recogniton_ppocr3();
    };
}
#endif
