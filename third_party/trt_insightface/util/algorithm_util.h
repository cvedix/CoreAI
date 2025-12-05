#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace trt_insightface {
    namespace util {
        // Calculate cosine similarity between two normalized embeddings
        float cosine_similarity(const std::vector<float>& emb1, const std::vector<float>& emb2);
        
        // Calculate L2 distance between two embeddings
        float l2_distance(const std::vector<float>& emb1, const std::vector<float>& emb2);
    }
}



