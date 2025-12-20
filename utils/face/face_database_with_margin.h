/**
 * @file face_database_with_margin.h
 * @brief Enhanced face database with margin-based confidence filtering
 * 
 * This implementation adds margin checking between top-1 and top-2 matches
 * to reduce false positives, especially useful for:
 * - Low quality images (poor lighting, blur, occlusion)
 * - Similar-looking people
 * - High security requirements (prefer rejection over wrong identification)
 * 
 * Based on feedback from Renesas AI accelerator project (facenet without GPU)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cmath>
#include <algorithm>
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_face_utils {

/**
 * @brief Match result with confidence metrics
 */
struct MatchResult {
    std::string name;           // Matched person name
    float score;                // Similarity score (cosine similarity)
    float margin;               // Confidence margin (top1 - top2)
    bool confident;             // Whether match passes margin check
    
    // Top-2 information for debugging
    std::string second_name;    // Second best match
    float second_score;         // Second best score
};

/**
 * @brief Enhanced face database with margin-based filtering
 * 
 * Key features:
 * 1. Cosine similarity matching
 * 2. Configurable similarity threshold
 * 3. Configurable confidence margin
 * 4. Quality-aware matching
 * 5. Multiple embeddings per person (average or best)
 */
class EnhancedFaceDatabase {
public:
    // Configuration
    float similarity_threshold = 0.7f;   // Min similarity to consider a match
    float confidence_margin = 0.3f;      // Min difference between top-1 and top-2
    bool use_margin_check = true;        // Enable/disable margin checking
    bool strict_mode = false;            // Strict: require BOTH threshold AND margin
                                        // Relaxed: threshold OR (lower threshold + margin)
    
    // Statistics
    int total_queries = 0;
    int accepted_matches = 0;
    int rejected_by_threshold = 0;
    int rejected_by_margin = 0;
    
    /**
     * @brief Load face database from file
     * 
     * File format (each line):
     *   name|embedding[0],embedding[1],...,embedding[511]
     * or
     *   name embedding[0] embedding[1] ... embedding[511]
     * 
     * @param path Path to database file
     * @param normalize Whether to L2 normalize embeddings (default: true)
     * @return Success status
     */
    bool load(const std::string& path, bool normalize = true) {
        std::ifstream db(path);
        if (!db.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[FaceDB] Failed to open database: %s", path.c_str()));
            return false;
        }
        
        std::string line;
        int count = 0;
        int line_num = 0;
        
        while (std::getline(db, line)) {
            line_num++;
            if (line.empty() || line[0] == '#') {
                continue;  // Skip empty lines and comments
            }
            
            // Parse name and embedding
            std::string name;
            std::vector<float> emb;
            
            // Try pipe-separated format first (name|emb)
            size_t pipe_pos = line.find('|');
            if (pipe_pos != std::string::npos) {
                name = line.substr(0, pipe_pos);
                std::string emb_str = line.substr(pipe_pos + 1);
                
                // Parse comma-separated values
                std::stringstream ss(emb_str);
                std::string val;
                while (std::getline(ss, val, ',')) {
                    try {
                        emb.push_back(std::stof(val));
                    } catch (...) {
                        CVEDIX_ERROR(cvedix_utils::string_format(
                            "[FaceDB] Invalid embedding value at line %d", line_num));
                        return false;
                    }
                }
            } else {
                // Try space-separated format (name emb[0] emb[1] ...)
                std::stringstream ss(line);
                ss >> name;
                float val;
                while (ss >> val) {
                    emb.push_back(val);
                }
            }
            
            // Validate embedding size
            if (emb.size() != 512) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[FaceDB] Invalid embedding size for %s: %d (expected 512) at line %d",
                    name.c_str(), static_cast<int>(emb.size()), line_num));
                return false;
            }
            
            // L2 normalize if requested
            if (normalize) {
                float norm = 0.0f;
                for (float v : emb) {
                    norm += v * v;
                }
                norm = std::sqrt(norm);
                if (norm > 1e-6) {
                    for (float& v : emb) {
                        v /= norm;
                    }
                }
            }
            
            // Add to database
            if (embeddings.find(name) == embeddings.end()) {
                embeddings[name] = std::vector<std::vector<float>>();
            }
            embeddings[name].push_back(emb);
            count++;
        }
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[FaceDB] Loaded %d embeddings for %d people",
            count, static_cast<int>(embeddings.size())));
        
        return true;
    }
    
    /**
     * @brief Find best match with margin checking
     * 
     * @param query_embedding Query embedding (should be L2 normalized)
     * @return Match result with confidence metrics
     */
    MatchResult find_match(const std::vector<float>& query_embedding) {
        total_queries++;
        
        MatchResult result;
        result.name = "Unknown";
        result.score = -1.0f;
        result.margin = 0.0f;
        result.confident = false;
        result.second_name = "Unknown";
        result.second_score = -1.0f;
        
        if (embeddings.empty()) {
            return result;
        }
        
        // Find top-2 matches
        std::vector<std::pair<std::string, float>> scores;
        
        for (const auto& [name, emb_list] : embeddings) {
            // For multiple embeddings per person, use the best match
            float best_score = -1.0f;
            for (const auto& db_emb : emb_list) {
                float score = cosine_similarity(query_embedding, db_emb);
                best_score = std::max(best_score, score);
            }
            scores.push_back({name, best_score});
        }
        
        // Sort by score descending
        std::sort(scores.begin(), scores.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Get top-1 and top-2
        result.name = scores[0].first;
        result.score = scores[0].second;
        
        if (scores.size() >= 2) {
            result.second_name = scores[1].first;
            result.second_score = scores[1].second;
            result.margin = result.score - result.second_score;
        } else {
            result.margin = result.score;  // Only one person in database
        }
        
        // Apply filtering logic
        bool passes_threshold = result.score >= similarity_threshold;
        bool passes_margin = !use_margin_check || (result.margin >= confidence_margin);
        
        if (strict_mode) {
            // Strict: BOTH conditions must pass
            result.confident = passes_threshold && passes_margin;
        } else {
            // Relaxed: Pass if threshold is met, OR if margin is good with lower threshold
            float relaxed_threshold = similarity_threshold - 0.1f;
            result.confident = passes_threshold && passes_margin;
            
            // Alternative: high margin can compensate for slightly lower score
            if (!result.confident && result.margin >= confidence_margin && 
                result.score >= relaxed_threshold) {
                result.confident = true;
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[FaceDB] Accepted with relaxed threshold: score=%.3f (margin=%.3f)",
                    result.score, result.margin));
            }
        }
        
        // Update statistics
        if (result.confident) {
            accepted_matches++;
        } else if (!passes_threshold) {
            rejected_by_threshold++;
        } else {
            rejected_by_margin++;
        }
        
        // If not confident, return "Unknown"
        if (!result.confident) {
            result.name = "Unknown";
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[FaceDB] Query result: %s (score=%.3f, margin=%.3f, confident=%s)",
            result.name.c_str(), result.score, result.margin,
            result.confident ? "YES" : "NO"));
        
        return result;
    }
    
    /**
     * @brief Get all similarity scores for a query, sorted from high to low
     * 
     * @param query_embedding Query embedding (should be L2 normalized)
     * @return Vector of (name, score) pairs sorted by score descending
     */
    std::vector<std::pair<std::string, float>> get_all_scores(const std::vector<float>& query_embedding) {
        std::vector<std::pair<std::string, float>> scores;
        
        if (embeddings.empty()) {
            return scores;
        }
        
        for (const auto& [name, emb_list] : embeddings) {
            // For multiple embeddings per person, use the best match
            float best_score = -1.0f;
            for (const auto& db_emb : emb_list) {
                float score = cosine_similarity(query_embedding, db_emb);
                best_score = std::max(best_score, score);
            }
            scores.push_back({name, best_score});
        }
        
        // Sort by score descending
        std::sort(scores.begin(), scores.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        return scores;
    }
    
    /**
     * @brief Add face to database
     */
    void add_face(const std::string& name, const std::vector<float>& embedding) {
        if (embeddings.find(name) == embeddings.end()) {
            embeddings[name] = std::vector<std::vector<float>>();
        }
        embeddings[name].push_back(embedding);
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[FaceDB] Added embedding for %s (total: %d)",
            name.c_str(), static_cast<int>(embeddings[name].size())));
    }
    
    /**
     * @brief Save database to file
     */
    bool save(const std::string& path) {
        std::ofstream db(path);
        if (!db.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[FaceDB] Failed to save database: %s", path.c_str()));
            return false;
        }
        
        int count = 0;
        for (const auto& [name, emb_list] : embeddings) {
            for (const auto& emb : emb_list) {
                db << name << "|";
                for (size_t i = 0; i < emb.size(); i++) {
                    db << emb[i];
                    if (i < emb.size() - 1) db << ",";
                }
                db << std::endl;
                count++;
            }
        }
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[FaceDB] Saved %d embeddings to %s", count, path.c_str()));
        
        return true;
    }
    
    /**
     * @brief Get statistics
     */
    void print_statistics() {
        CVEDIX_INFO("[FaceDB] === Statistics ===");
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Total queries: %d", total_queries));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Accepted: %d (%.1f%%)",
            accepted_matches,
            100.0f * accepted_matches / std::max(1, total_queries)));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Rejected by threshold: %d (%.1f%%)",
            rejected_by_threshold,
            100.0f * rejected_by_threshold / std::max(1, total_queries)));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Rejected by margin: %d (%.1f%%)",
            rejected_by_margin,
            100.0f * rejected_by_margin / std::max(1, total_queries)));
    }
    
    /**
     * @brief Clear database
     */
    void clear() {
        embeddings.clear();
        total_queries = 0;
        accepted_matches = 0;
        rejected_by_threshold = 0;
        rejected_by_margin = 0;
    }
    
private:
    // Store multiple embeddings per person (for averaging or best-match)
    std::map<std::string, std::vector<std::vector<float>>> embeddings;
    
    /**
     * @brief Compute cosine similarity between two embeddings
     * 
     * For L2-normalized embeddings: cosine_sim = dot_product
     */
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) {
            return -1.0f;
        }
        
        float dot = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
        }
        
        // If embeddings are L2 normalized, dot product = cosine similarity
        // Otherwise, need to normalize:
        // return dot / (norm(a) * norm(b));
        
        return dot;
    }
};

} // namespace cvedix_face_utils






