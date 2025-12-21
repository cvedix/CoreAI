/**
 * @file cvedix_ppocr_text_detector_node.h
 * @brief PaddleOCR text detection and recognition
 * 
 * OCR node using PaddlePaddle framework (not OpenCV DNN).
 * 
 * @section prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_PADDLE`
 * 
 * @see https://github.com/PaddlePaddle/PaddleOCR
 */

#pragma once

#ifdef CVEDIX_WITH_PADDLE
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/third_party/paddle_ocr/include/paddleocr.h"

namespace cvedix_nodes {
    /**
     * @brief PaddleOCR text detector/recognizer
     * 
     * @note Uses Paddle framework directly, not OpenCV DNN
     */
    class cvedix_ppocr_text_detector_node: public cvedix_primary_infer_node
    {

    private:
        // paddle ocr instance
        std::shared_ptr<PaddleOCR::PPOCR> ocr;
    protected:
        // we need a totally new logic for the whole infer combinations
        // no separate step pre-defined needed in base class
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        // override pure virtual method, for compile pass
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_ppocr_text_detector_node(std::string node_name, 
                                    std::string det_model_dir = "", 
                                    std::string cls_model_dir = "", 
                                    std::string rec_model_dir = "", 
                                    std::string rec_char_dict_path = "");
        ~cvedix_ppocr_text_detector_node();
    };
}
#endif