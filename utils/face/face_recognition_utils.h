/**
 * @file face_recognition_utils.h
 * @brief Advanced utilities for face recognition: Voting, TTA, ID-Specific Threshold
 * 
 * These techniques improve recognition accuracy in real-world scenarios.
 */

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <numeric>

namespace cvedix_face_utils {

/**
 * @brief Temporal Voting Buffer for stable face recognition
 * 
 * Collects recognition results over time and returns the majority vote.
 * This reduces flickering and improves stability in video streams.
 * 
 * Example: If last 10 frames show [A,A,B,A,A,A,C,A,A,A], result is "A"
 */
class VotingBuffer {
public:
    struct VoteEntry {
        std::string name;
        float score;
        int track_id;  // Track ID to separate different faces
    };
    
    /**
     * @brief Constructor
     * @param window_size Number of frames to consider for voting
     * @param majority_threshold Percentage required for confident result (0.0-1.0)
     */
    VotingBuffer(int window_size = 10, float majority_threshold = 0.5f)
        : window_size_(window_size), majority_threshold_(majority_threshold) {}
    
    /**
     * @brief Add a vote to the buffer
     * @param track_id Unique ID for the face track
     * @param name Recognition result
     * @param score Confidence score
     */
    void add_vote(int track_id, const std::string& name, float score) {
        auto& buffer = track_buffers_[track_id];
        buffer.push_back({name, score, track_id});
        
        // Keep buffer size limited
        while (buffer.size() > static_cast<size_t>(window_size_)) {
            buffer.pop_front();
        }
    }
    
    /**
     * @brief Get stable result based on majority voting
     * @param track_id Face track ID
     * @return Pair of (name, confidence), or ("Unknown", 0.0) if no majority
     */
    std::pair<std::string, float> get_stable_result(int track_id) {
        auto it = track_buffers_.find(track_id);
        if (it == track_buffers_.end() || it->second.empty()) {
            return {"Unknown", 0.0f};
        }
        
        const auto& buffer = it->second;
        
        // Count votes
        std::map<std::string, int> vote_count;
        std::map<std::string, float> score_sum;
        
        for (const auto& entry : buffer) {
            vote_count[entry.name]++;
            score_sum[entry.name] += entry.score;
        }
        
        // Find winner
        std::string winner = "Unknown";
        int max_votes = 0;
        float winner_score = 0.0f;
        
        for (const auto& [name, count] : vote_count) {
            if (count > max_votes) {
                max_votes = count;
                winner = name;
                winner_score = score_sum[name] / count;  // Average score
            }
        }
        
        // Check majority threshold
        float vote_ratio = static_cast<float>(max_votes) / buffer.size();
        if (vote_ratio >= majority_threshold_) {
            return {winner, winner_score};
        }
        
        return {"Unknown", 0.0f};  // No majority
    }
    
    /**
     * @brief Clear buffer for a specific track
     */
    void clear_track(int track_id) {
        track_buffers_.erase(track_id);
    }
    
    /**
     * @brief Clear all buffers
     */
    void clear_all() {
        track_buffers_.clear();
    }
    
    // Configuration
    void set_window_size(int size) { window_size_ = size; }
    void set_majority_threshold(float threshold) { majority_threshold_ = threshold; }
    int get_window_size() const { return window_size_; }
    float get_majority_threshold() const { return majority_threshold_; }
    
private:
    int window_size_;
    float majority_threshold_;
    std::map<int, std::deque<VoteEntry>> track_buffers_;
};

/**
 * @brief TTA (Test Time Augmentation) configuration
 * 
 * Generates multiple augmented versions of input face for more robust matching.
 * Trade-off: Higher accuracy, lower FPS.
 */
struct TTAConfig {
    bool enabled = false;           // Default OFF (performance priority)
    bool use_flip = true;           // Horizontal flip
    bool use_brightness = false;    // Brightness variation
    float brightness_delta = 0.1f;  // ±10% brightness
    
    /**
     * @brief Get number of augmented images to generate
     */
    int augmentation_count() const {
        if (!enabled) return 1;
        int count = 1;  // Original
        if (use_flip) count++;
        if (use_brightness) count += 2;  // +bright, -bright
        return count;
    }
};

/**
 * @brief Helper to average multiple embeddings (for TTA)
 */
inline std::vector<float> average_embeddings(
    const std::vector<std::vector<float>>& embeddings) {
    
    if (embeddings.empty()) return {};
    if (embeddings.size() == 1) return embeddings[0];
    
    size_t dim = embeddings[0].size();
    std::vector<float> result(dim, 0.0f);
    
    for (const auto& emb : embeddings) {
        for (size_t i = 0; i < dim && i < emb.size(); i++) {
            result[i] += emb[i];
        }
    }
    
    float n = static_cast<float>(embeddings.size());
    for (float& val : result) {
        val /= n;
    }
    
    // L2 normalize the averaged embedding
    float norm = 0.0f;
    for (float val : result) norm += val * val;
    norm = std::sqrt(norm);
    if (norm > 1e-6f) {
        for (float& val : result) val /= norm;
    }
    
    return result;
}

/**
 * @brief Recognition configuration options
 * 
 * Allows users to enable/disable different accuracy improvement techniques.
 * Default: Only voting enabled (lightweight, good balance)
 */
struct RecognitionConfig {
    // Temporal Voting (Default: ON - lightweight, improves stability)
    bool voting_enabled = true;
    int voting_window_size = 10;
    float voting_majority_threshold = 0.5f;
    
    // TTA - Test Time Augmentation (Default: OFF - reduces FPS)
    TTAConfig tta;
    
    // ID-Specific Threshold (Default: OFF - use global threshold)
    bool id_specific_threshold_enabled = false;
    
    // Global thresholds (used when ID-specific is OFF)
    float similarity_threshold = 0.7f;
    float confidence_margin = 0.3f;
    
    /**
     * @brief Create default config (balanced mode)
     */
    static RecognitionConfig balanced() {
        RecognitionConfig cfg;
        cfg.voting_enabled = true;
        cfg.tta.enabled = false;
        cfg.id_specific_threshold_enabled = false;
        return cfg;
    }
    
    /**
     * @brief Create high-accuracy config (for 1:1 verification)
     */
    static RecognitionConfig high_accuracy() {
        RecognitionConfig cfg;
        cfg.voting_enabled = true;
        cfg.voting_window_size = 15;
        cfg.voting_majority_threshold = 0.6f;
        cfg.tta.enabled = true;
        cfg.tta.use_flip = true;
        cfg.id_specific_threshold_enabled = true;
        cfg.similarity_threshold = 0.75f;
        cfg.confidence_margin = 0.35f;
        return cfg;
    }
    
    /**
     * @brief Create fast config (real-time priority)
     */
    static RecognitionConfig fast() {
        RecognitionConfig cfg;
        cfg.voting_enabled = false;
        cfg.tta.enabled = false;
        cfg.id_specific_threshold_enabled = false;
        cfg.similarity_threshold = 0.65f;
        return cfg;
    }
};

} // namespace cvedix_face_utils
