#include "algorithm_util.h"
#include <cmath>
#include <algorithm>

namespace trt_insightface {
    namespace util {
        float cosine_similarity(const std::vector<float>& emb1, const std::vector<float>& emb2) {
            if (emb1.size() != emb2.size()) {
                return -1.0f;
            }

            float dot_product = 0.0f;
            float norm1 = 0.0f;
            float norm2 = 0.0f;

            for (size_t i = 0; i < emb1.size(); i++) {
                dot_product += emb1[i] * emb2[i];
                norm1 += emb1[i] * emb1[i];
                norm2 += emb2[i] * emb2[i];
            }

            norm1 = std::sqrt(norm1);
            norm2 = std::sqrt(norm2);

            if (norm1 < 1e-6 || norm2 < 1e-6) {
                return 0.0f;
            }

            return dot_product / (norm1 * norm2);
        }

        float l2_distance(const std::vector<float>& emb1, const std::vector<float>& emb2) {
            if (emb1.size() != emb2.size()) {
                return 1e10f;  // Large value for invalid
            }

            float sum = 0.0f;
            for (size_t i = 0; i < emb1.size(); i++) {
                float diff = emb1[i] - emb2[i];
                sum += diff * diff;
            }

            return std::sqrt(sum);
        }
    }
}






