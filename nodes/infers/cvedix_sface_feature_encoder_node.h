
#pragma once

#include "base/cvedix_secondary_infer_node.h"

namespace cvedix_nodes {
    // face feature encoder based on SFace, update embeddings of cvedix_frame_face_target
    // https://github.com/opencv/opencv/blob/4.x/modules/objdetect/src/face_recognize.cpp
    // https://github.com/zhongyy/SFace
    class cvedix_sface_feature_encoder_node: public cvedix_secondary_infer_node
    {
    private:
        // get transform matrix for aglin face
        cv::Mat getSimilarityTransformMatrix(float src[5][2]);
        // align and crop
        void alignCrop(cv::Mat& _src_img, float _src_point[5][2], cv::Mat& _aligned_img);
    protected:
        // override prepare as sface has an additional logic for face align before preprocess
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, std::vector<cv::Mat>& mats_to_infer) override;

        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_sface_feature_encoder_node(std::string node_name, std::string model_path);
        ~cvedix_sface_feature_encoder_node();
    };

 }