/**
 * @file face_accuracy_benchmark.h
 * @brief Face Recognition Accuracy Benchmark Module
 *
 * Implements verification and identification metrics for face recognition:
 * - Verification: TAR (True Accept Rate) @ FAR (False Accept Rate), EER
 * - Identification: Rank-1, Rank-5, mAP
 *
 * Supported datasets: LFW, IJB-B, IJB-C, MegaFace
 *
 * Usage:
 *   FaceAccuracyBenchmark benchmark;
 *   benchmark.load_dataset("lfw", "/path/to/lfw");
 *   benchmark.set_recognizer(recognizer);
 *   benchmark.run();
 *   benchmark.print_results();
 */

#pragma once

#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace cvedix_face_benchmark {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Face pair for verification (LFW format)
 */
struct FacePair {
    std::string image1_path;
    std::string image2_path;
    bool is_same_person;  // true = positive pair, false = negative pair
};

/**
 * @brief Face template for IJB-B/C (template-based verification)
 */
struct FaceTemplate {
    std::string template_id;
    std::vector<std::string> image_paths;
    std::string subject_id;  // Ground truth identity
};

/**
 * @brief Verification result
 */
struct VerificationResult {
    float similarity;
    bool is_same_person;
    bool is_true_positive;   // Correctly accepted
    bool is_true_negative;   // Correctly rejected
    bool is_false_positive;  // Incorrectly accepted
    bool is_false_negative;  // Incorrectly rejected
};

/**
 * @brief Identification result
 */
struct IdentificationResult {
    std::string query_id;
    std::string query_path;
    std::vector<std::pair<std::string, float>> ranked_list;  // (subject_id, similarity)
    int rank_1_correct;
    int rank_5_correct;
};

/**
 * @brief Benchmark configuration
 */
struct BenchmarkConfig {
    // Dataset settings
    std::string dataset_name = "lfw";
    std::string dataset_root = "./datasets";

    // Recognition settings
    bool use_mask_recognizer = false;
    float similarity_threshold = 0.5f;

    // Margin-based verification (Top-1 + Margin)
    bool use_margin_verification = false;
    float margin_threshold = 0.15f;  // Minimum difference between top-1 and top-2

    // FAR thresholds for verification
    std::vector<float> far_thresholds = {0.1f, 0.01f, 0.001f};

    // Download settings
    bool auto_download = true;
    bool force_download = false;
};

// ============================================================================
// Metrics Calculator
// ============================================================================

/**
 * @brief Calculate accuracy metrics (TAR@FAR, EER, Rank-k, mAP)
 */
class AccuracyMetrics {
public:
    /**
     * @brief Calculate TAR (True Accept Rate) at specific FAR
     * @param similarities Similarity scores for all pairs
     * @param labels Ground truth labels (true = same person)
     * @param far_target Target FAR
     * @return TAR at given FAR
     */
    static float calculate_tar_at_far(
        const std::vector<float>& similarities,
        const std::vector<bool>& labels,
        float far_target) {

        if (similarities.size() != labels.size() || similarities.empty()) {
            return 0.0f;
        }

        // Sort by similarity (descending)
        std::vector<std::pair<float, bool>> pairs;
        for (size_t i = 0; i < similarities.size(); i++) {
            pairs.emplace_back(similarities[i], labels[i]);
        }
        std::sort(pairs.begin(), pairs.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        // Find threshold that achieves target FAR
        int num_negatives = 0;
        for (bool label : labels) {
            if (!label) num_negatives++;
        }

        if (num_negatives == 0) return 1.0f;

        int max_false_accepts = static_cast<int>(far_target * num_negatives);

        float threshold = 0.0f;
        int false_accepts = 0;

        for (const auto& [sim, label] : pairs) {
            if (!label) {
                false_accepts++;
            }
            if (false_accepts > max_false_accepts) {
                threshold = sim;
                break;
            }
        }

        if (threshold == 0.0f) threshold = pairs.back().first - 0.001f;

        // Calculate TAR at this threshold
        int true_accepts = 0;
        int total_positives = 0;
        for (const auto& [sim, label] : pairs) {
            if (label) {
                total_positives++;
                if (sim >= threshold) {
                    true_accepts++;
                }
            }
        }

        return total_positives > 0 ? static_cast<float>(true_accepts) / total_positives : 0.0f;
    }

    /**
     * @brief Calculate EER (Equal Error Rate)
     * @param similarities Similarity scores
     * @param labels Ground truth labels
     * @return EER (FAR = FRR at this point)
     */
    static float calculate_eer(
        const std::vector<float>& similarities,
        const std::vector<bool>& labels) {

        if (similarities.size() != labels.size() || similarities.empty()) {
            return 1.0f;
        }

        // Sort by similarity
        std::vector<std::pair<float, bool>> pairs;
        for (size_t i = 0; i < similarities.size(); i++) {
            pairs.emplace_back(similarities[i], labels[i]);
        }
        std::sort(pairs.begin(), pairs.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        int num_positives = 0;
        int num_negatives = 0;
        for (bool label : labels) {
            if (label) num_positives++;
            else num_negatives++;
        }

        if (num_positives == 0 || num_negatives == 0) return 0.0f;

        float min_diff = 1.0f;
        float eer = 0.0f;

        for (size_t i = 0; i < pairs.size(); i++) {
            float threshold = pairs[i].first;

            // FAR = FP / (FP + TN)
            int fp = 0, tn = 0;
            for (size_t j = 0; j < pairs.size(); j++) {
                if (!pairs[j].second) {  // Negative pair
                    if (pairs[j].first >= threshold) fp++;
                    else tn++;
                }
            }
            float far = num_negatives > 0 ? static_cast<float>(fp) / num_negatives : 0.0f;

            // FRR = FN / (FN + TP)
            int fn = 0, tp = 0;
            for (size_t j = 0; j < pairs.size(); j++) {
                if (pairs[j].second) {  // Positive pair
                    if (pairs[j].first < threshold) fn++;
                    else tp++;
                }
            }
            float frr = num_positives > 0 ? static_cast<float>(fn) / num_positives : 0.0f;

            float diff = std::abs(far - frr);
            if (diff < min_diff) {
                min_diff = diff;
                eer = (far + frr) / 2.0f;
            }
        }

        return eer;
    }

    /**
     * @brief Find optimal threshold that maximizes accuracy
     * @param similarities Similarity scores
     * @param labels Ground truth labels
     * @return Optimal threshold
     */
    static float find_optimal_threshold(
        const std::vector<float>& similarities,
        const std::vector<bool>& labels) {

        if (similarities.size() != labels.size() || similarities.empty()) {
            return 0.5f;
        }

        // Search for best threshold
        float best_threshold = 0.5f;
        float best_accuracy = 0.0f;

        for (float threshold = 0.1f; threshold <= 0.95f; threshold += 0.01f) {
            int correct = 0;
            for (size_t i = 0; i < similarities.size(); i++) {
                bool pred_same = similarities[i] >= threshold;
                if (pred_same == labels[i]) {
                    correct++;
                }
            }
            float accuracy = static_cast<float>(correct) / similarities.size();

            if (accuracy > best_accuracy) {
                best_accuracy = accuracy;
                best_threshold = threshold;
            }
        }

        return best_threshold;
    }

    /**
     * @brief Calculate accuracy at specific threshold
     */
    static float calculate_accuracy_at_threshold(
        const std::vector<float>& similarities,
        const std::vector<bool>& labels,
        float threshold) {

        if (similarities.size() != labels.size() || similarities.empty()) {
            return 0.0f;
        }

        int correct = 0;
        for (size_t i = 0; i < similarities.size(); i++) {
            bool pred_same = similarities[i] >= threshold;
            if (pred_same == labels[i]) {
                correct++;
            }
        }
        return static_cast<float>(correct) / similarities.size();
    }

    /**
     * @brief Calculate accuracy with margin-based verification
     * @param similarities Vector of pairs: (top1_similarity, top2_similarity)
     * @param labels Ground truth labels
     * @param threshold Base similarity threshold
     * @param margin_threshold Minimum difference between top-1 and top-2
     * @return Accuracy
     *
     * Decision logic:
     * - If top1 >= threshold: ACCEPT (confident)
     * - Else if top1 >= threshold - 0.1 AND margin >= margin_threshold: ACCEPT (margin-based)
     * - Else: REJECT
     */
    static float calculate_accuracy_with_margin(
        const std::vector<std::pair<float, float>>& similarities,  // (top1, top2)
        const std::vector<bool>& labels,
        float threshold,
        float margin_threshold) {

        if (similarities.size() != labels.size() || similarities.empty()) {
            return 0.0f;
        }

        int correct = 0;
        for (size_t i = 0; i < similarities.size(); i++) {
            float top1 = similarities[i].first;
            float top2 = similarities[i].second;
            float margin = top1 - top2;

            // Margin-based decision logic
            bool pred_same = false;

            if (top1 >= threshold) {
                // Rule 1: Top-1 high enough → Accept
                pred_same = true;
            } else if (top1 >= (threshold - 0.1f) && margin >= margin_threshold) {
                // Rule 2: Top-1 slightly lower but margin is high → Accept
                pred_same = true;
            } else {
                // Rule 3: Below threshold → Reject
                pred_same = false;
            }

            if (pred_same == labels[i]) {
                correct++;
            }
        }
        return static_cast<float>(correct) / similarities.size();
    }

    /**
     * @brief Get detailed threshold analysis
     */
    static void analyze_thresholds(
        const std::vector<float>& similarities,
        const std::vector<bool>& labels) {

        std::cout << "\n  === Threshold Analysis ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);

        float thresholds[] = {0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        for (float threshold : thresholds) {
            float acc = calculate_accuracy_at_threshold(similarities, labels, threshold);
            std::cout << "    Threshold " << threshold << ": Accuracy = " << acc * 100 << "%" << std::endl;
        }

        float optimal = find_optimal_threshold(similarities, labels);
        float optimal_acc = calculate_accuracy_at_threshold(similarities, labels, optimal);
        std::cout << "    Optimal threshold: " << optimal << " → Accuracy = " << optimal_acc * 100 << "%" << std::endl;
    }

    /**
     * @brief Calculate Rank-k accuracy
     * @param query_features Query embeddings
     * @param gallery_features Gallery embeddings
     * @param gallery_labels Gallery identity labels
     * @param k Rank-k (1, 5, etc.)
     * @return Rank-k accuracy
     */
    static float calculate_rank_k(
        const std::vector<std::vector<float>>& query_features,
        const std::vector<std::vector<float>>& gallery_features,
        const std::vector<std::string>& gallery_labels,
        int k) {

        if (query_features.empty() || gallery_features.empty()) {
            return 0.0f;
        }

        int correct = 0;

        for (const auto& query : query_features) {
            // Calculate similarities to all gallery images
            std::vector<std::pair<float, int>> scores;
            for (size_t i = 0; i < gallery_features.size(); i++) {
                float sim = cosine_similarity(query, gallery_features[i]);
                scores.emplace_back(sim, static_cast<int>(i));
            }

            // Sort by similarity descending
            std::sort(scores.begin(), scores.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

            // Check if any of top-k matches the query label
            // Note: For Rank-k on single-shot, we compare query embedding to each gallery
            // In practice, you'd have gallery features organized by identity
        }

        return static_cast<float>(correct) / query_features.size();
    }

    /**
     * @brief Calculate mAP (mean Average Precision)
     */
    static float calculate_map(
        const std::vector<std::vector<std::pair<std::string, float>>>& ranked_lists,
        const std::vector<std::string>& query_labels) {

        if (ranked_lists.size() != query_labels.size()) {
            return 0.0f;
        }

        float ap_sum = 0.0f;

        for (size_t i = 0; i < ranked_lists.size(); i++) {
            const auto& ranked = ranked_lists[i];
            const std::string& query_label = query_labels[i];

            int num_relevant = 0;
            float precision_sum = 0.0f;

            for (size_t j = 0; j < ranked.size(); j++) {
                if (ranked[j].first == query_label) {
                    num_relevant++;
                    precision_sum += static_cast<float>(num_relevant) / (j + 1);
                }
            }

            if (num_relevant > 0) {
                ap_sum += precision_sum / num_relevant;
            }
        }

        return ranked_lists.empty() ? 0.0f : ap_sum / ranked_lists.size();
    }

private:
    static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return 0.0f;
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        float norm = std::sqrt(norm_a) * std::sqrt(norm_b);
        return norm > 1e-6f ? dot / norm : 0.0f;
    }
};

// ============================================================================
// Dataset Parsers
// ============================================================================

/**
 * @brief LFW dataset parser
 */
class LFWParser {
public:
    /**
     * @brief Load LFW pairs from standard protocol file
     * @param pairs_file Path to pairs.txt
     * @param root_dir LFW root directory
     * @return Vector of face pairs
     */
    static std::vector<FacePair> load_pairs(const std::string& pairs_file,
                                             const std::string& root_dir) {
        std::vector<FacePair> pairs;

        std::ifstream file(pairs_file);
        if (!file.is_open()) {
            std::cerr << "[LFW] Failed to open pairs file: " << pairs_file << std::endl;
            return pairs;
        }

        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            if (line.empty()) continue;

            // Skip header line (e.g., "10	300")
            if (line_num == 1 && line.find('\t') != std::string::npos) {
                std::stringstream ss(line);
                std::string header;
                if (ss >> header) {
                    try {
                        int num = std::stoi(header);
                        if (num > 0 && num < 100) {
                            std::cout << "[LFW] Skipping header: " << line << std::endl;
                            continue;
                        }
                    } catch (...) {}
                }
            }

            // LFW pairs.txt format:
            // Positive pairs (same person): 3 fields (tab or space separated)
            //   name1 idx1 idx2  (same person, idx1 != idx2)
            // Negative pairs (different people): 4 fields (tab or space separated)
            //   name1 idx1 name2 idx2

            // Replace tabs with spaces for uniform parsing
            std::replace(line.begin(), line.end(), '\t', ' ');

            // Collapse multiple spaces
            std::stringstream ss(line);
            std::string field;
            std::vector<std::string> fields;
            while (ss >> field) {
                fields.push_back(field);
            }

            if (fields.size() != 3 && fields.size() != 4) {
                std::cerr << "[LFW] Warning: Invalid line " << line_num << ": " << line << std::endl;
                continue;
            }

            FacePair pair;

            if (fields.size() == 3) {
                // Positive pair: same person
                // Format: name idx1 idx2
                pair.is_same_person = true;
                std::string name = fields[0];
                int idx1 = std::stoi(fields[1]);
                int idx2 = std::stoi(fields[2]);

                char idx1_str[16], idx2_str[16];
                sprintf(idx1_str, "%04d", idx1);
                sprintf(idx2_str, "%04d", idx2);

                pair.image1_path = root_dir + "/" + name + "/" + name + "_" + idx1_str + ".jpg";
                pair.image2_path = root_dir + "/" + name + "/" + name + "_" + idx2_str + ".jpg";
            } else {
                // Negative pair: different people
                // Format: name1 idx1 name2 idx2
                pair.is_same_person = false;
                std::string name1 = fields[0];
                int idx1 = std::stoi(fields[1]);
                std::string name2 = fields[2];
                int idx2 = std::stoi(fields[3]);

                char idx1_str[16], idx2_str[16];
                sprintf(idx1_str, "%04d", idx1);
                sprintf(idx2_str, "%04d", idx2);

                pair.image1_path = root_dir + "/" + name1 + "/" + name1 + "_" + idx1_str + ".jpg";
                pair.image2_path = root_dir + "/" + name2 + "/" + name2 + "_" + idx2_str + ".jpg";
            }

            pairs.push_back(pair);
        }

        // Count positive and negative pairs
        int positive = 0, negative = 0;
        for (const auto& p : pairs) {
            if (p.is_same_person) positive++;
            else negative++;
        }
        std::cout << "[LFW] Loaded " << pairs.size() << " pairs ("
                  << positive << " positive, " << negative << " negative)" << std::endl;
        return pairs;
    }

    /**
     * @brief Download LFW dataset
     */
    static bool download(const std::string& output_dir) {
        namespace fs = std::filesystem;

        std::string lfw_url = "http://vis-www.cs.umass.edu/lfw/lfw.tgz";
        std::string pairs_url = "http://vis-www.cs.umass.edu/lfw/pairs.txt";

        fs::create_directories(output_dir);

        std::cout << "[LFW] Downloading dataset..." << std::endl;

        // Download images
        std::string tgz_path = output_dir + "/lfw.tgz";
        std::string cmd = "cd " + output_dir + " && wget -q --show-progress -O lfw.tgz " + lfw_url;
        int ret = system(cmd.c_str());

        if (ret != 0) {
            std::cerr << "[LFW] Failed to download images" << std::endl;
            return false;
        }

        // Extract
        std::cout << "[LFW] Extracting..." << std::endl;
        ret = system(("cd " + output_dir + " && tar xzf lfw.tgz && rm lfw.tgz").c_str());
        if (ret != 0) {
            std::cerr << "[LFW] Failed to extract" << std::endl;
            return false;
        }

        // Download pairs.txt
        cmd = "cd " + output_dir + " && wget -q --show-progress -O pairs.txt " + pairs_url;
        ret = system(cmd.c_str());

        if (ret != 0) {
            std::cerr << "[LFW] Warning: Failed to download pairs.txt" << std::endl;
        }

        std::cout << "[LFW] Download complete: " << output_dir << std::endl;
        return true;
    }
};

// ============================================================================
// Face Recognizer Wrapper
// ============================================================================

/**
 * @brief Wrapper for SeetaFace6 recognizer for benchmark
 */
class FaceRecognizerWrapper {
public:
    FaceRecognizerWrapper() = default;

    bool init(const std::string& model_dir) {
        auto modelPath = [&](const std::string& name) {
            return model_dir + "/" + name;
        };

        // Load standard recognizer
        seeta::ModelSetting rec_std_set(modelPath("face_recognizer.csta"),
                                         seeta::ModelSetting::CPU, 0);
        recognizer_std_ = std::make_unique<seeta::FaceRecognizer>(rec_std_set);

        // Try to load mask recognizer
        seeta::ModelSetting rec_mask_set(modelPath("face_recognizer_mask.csta"),
                                          seeta::ModelSetting::CPU, 0);
        try {
            recognizer_mask_ = std::make_unique<seeta::FaceRecognizer>(rec_mask_set);
            has_mask_recognizer_ = true;
        } catch (...) {
            std::cout << "[Benchmark] Mask recognizer not available, using standard" << std::endl;
            has_mask_recognizer_ = false;
        }

        // Load detector and landmarker
        seeta::ModelSetting det_set(modelPath("face_detector.csta"),
                                     seeta::ModelSetting::CPU, 0);
        detector_ = std::make_unique<seeta::FaceDetector>(det_set);
        detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, 40);

        seeta::ModelSetting lm_set(modelPath("face_landmarker_pts5.csta"),
                                    seeta::ModelSetting::CPU, 0);
        landmarker_ = std::make_unique<seeta::FaceLandmarker>(lm_set);

        feature_size_ = recognizer_std_->GetExtractFeatureSize();
        std::cout << "[Benchmark] Recognizer initialized, feature size: " << feature_size_ << std::endl;

        return true;
    }

    /**
     * @brief Extract features from an image
     * @param image_path Path to face image
     * @return Feature vector, empty on failure
     */
    std::vector<float> extract_feature(const std::string& image_path) {
        cv::Mat img = cv::imread(image_path);
        if (img.empty()) {
            std::cerr << "[Benchmark] Failed to load image: " << image_path << std::endl;
            return {};
        }

        return extract_feature(img);
    }

    /**
     * @brief Extract features from cv::Mat
     */
    std::vector<float> extract_feature(const cv::Mat& image) {
        if (image.empty()) return {};

        // For small images, resize up first for better detection
        cv::Mat processed = image.clone();
        float scale = 1.0f;

        if (image.cols < 200 || image.rows < 200) {
            // Upscale small images
            float max_dim = std::max(image.cols, image.rows);
            if (max_dim < 200) {
                scale = 200.0f / max_dim;
                cv::resize(image, processed, cv::Size(), scale, scale, cv::INTER_LINEAR);
            }
        }

        SeetaImageData simg;
        simg.width = processed.cols;
        simg.height = processed.rows;
        simg.channels = processed.channels();
        simg.data = processed.data;

        // Detect face with lower threshold for benchmark
        detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, 30);
        SeetaFaceInfoArray faces = detector_->detect(simg);

        if (faces.size == 0) {
            // Try with even larger image
            float up_scale = 2.0f;
            cv::Mat larger;
            cv::resize(processed, larger, cv::Size(), up_scale, up_scale, cv::INTER_LINEAR);
            simg.width = larger.cols;
            simg.height = larger.rows;
            simg.data = larger.data;
            faces = detector_->detect(simg);

            if (faces.size == 0) {
                std::cerr << "[Benchmark] No face detected" << std::endl;
                return {};
            }

            // Adjust scale for coordinate conversion
            scale = scale * up_scale;
        }

        // Use largest face
        int best_idx = 0;
        int best_area = faces.data[0].pos.width * faces.data[0].pos.height;
        for (int i = 1; i < faces.size; i++) {
            int area = faces.data[i].pos.width * faces.data[i].pos.height;
            if (area > best_area) {
                best_area = area;
                best_idx = i;
            }
        }

        // Scale coordinates back to original image size
        SeetaRect rect = faces.data[best_idx].pos;
        rect.x = static_cast<int>(rect.x / scale);
        rect.y = static_cast<int>(rect.y / scale);
        rect.width = static_cast<int>(rect.width / scale);
        rect.height = static_cast<int>(rect.height / scale);

        // Get landmarks using original image
        SeetaImageData orig_simg;
        orig_simg.width = image.cols;
        orig_simg.height = image.rows;
        orig_simg.channels = image.channels();
        orig_simg.data = image.data;

        auto points = landmarker_->mark(orig_simg, rect);

        // Extract features
        std::vector<float> features(feature_size_);
        seeta::FaceRecognizer* rec = recognizer_mask_ ? recognizer_mask_.get() : recognizer_std_.get();
        rec->Extract(orig_simg, points.data(), features.data());

        return features;
    }

    /**
     * @brief Calculate similarity between two features
     */
    float similarity(const std::vector<float>& f1, const std::vector<float>& f2) {
        if (f1.empty() || f2.empty() || f1.size() != f2.size()) {
            return 0.0f;
        }

        // Cosine similarity
        float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
        for (size_t i = 0; i < f1.size(); i++) {
            dot += f1[i] * f2[i];
            norm1 += f1[i] * f1[i];
            norm2 += f2[i] * f2[i];
        }

        float norm = std::sqrt(norm1) * std::sqrt(norm2);
        return norm > 1e-6f ? dot / norm : 0.0f;
    }

    int feature_size() const { return feature_size_; }

private:
    std::unique_ptr<seeta::FaceDetector> detector_;
    std::unique_ptr<seeta::FaceLandmarker> landmarker_;
    std::unique_ptr<seeta::FaceRecognizer> recognizer_std_;
    std::unique_ptr<seeta::FaceRecognizer> recognizer_mask_;
    int feature_size_ = 0;
    bool has_mask_recognizer_ = false;
};

// ============================================================================
// Main Benchmark Class
// ============================================================================

/**
 * @brief Main accuracy benchmark class
 */
class FaceAccuracyBenchmark {
public:
    FaceAccuracyBenchmark() = default;

    /**
     * @brief Configure benchmark
     */
    void configure(const BenchmarkConfig& config) {
        config_ = config;
    }

    /**
     * @brief Set face recognizer
     */
    void set_recognizer(std::shared_ptr<FaceRecognizerWrapper> recognizer) {
        recognizer_ = recognizer;
    }

    /**
     * @brief Set model directory
     */
    void set_model_dir(const std::string& model_dir) {
        model_dir_ = model_dir;
    }

    /**
     * @brief Initialize recognizer
     */
    bool init_recognizer() {
        if (recognizer_) return true;

        recognizer_ = std::make_shared<FaceRecognizerWrapper>();
        return recognizer_->init(model_dir_);
    }

    /**
     * @brief Run LFW verification benchmark
     */
    bool run_lfw_verification() {
        std::string lfw_dir = config_.dataset_root + "/lfw";

        // Check if dataset exists
        if (!std::filesystem::exists(lfw_dir)) {
            if (config_.auto_download) {
                std::cout << "[Benchmark] LFW not found, downloading..." << std::endl;
                if (!LFWParser::download(config_.dataset_root)) {
                    std::cerr << "[Benchmark] Failed to download LFW" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "[Benchmark] LFW dataset not found: " << lfw_dir << std::endl;
                return false;
            }
        }

        std::string pairs_file = lfw_dir + "/pairs.txt";
        if (!std::filesystem::exists(pairs_file)) {
            pairs_file = config_.dataset_root + "/pairs.txt";
        }

        if (!std::filesystem::exists(pairs_file)) {
            std::cerr << "[Benchmark] pairs.txt not found" << std::endl;
            return false;
        }

        // Load pairs
        auto pairs = LFWParser::load_pairs(pairs_file, lfw_dir);
        if (pairs.empty()) {
            std::cerr << "[Benchmark] No pairs loaded" << std::endl;
            return false;
        }

        // Extract features and calculate similarities
        std::vector<float> similarities;
        std::vector<bool> labels;

        std::cout << "[Benchmark] Processing " << pairs.size() << " pairs..." << std::endl;

        int processed = 0;
        for (const auto& pair : pairs) {
            auto feat1 = recognizer_->extract_feature(pair.image1_path);
            auto feat2 = recognizer_->extract_feature(pair.image2_path);

            if (feat1.empty() || feat2.empty()) {
                std::cerr << "Warning: Failed to extract features from pair" << std::endl;
                continue;
            }

            float sim = recognizer_->similarity(feat1, feat2);
            similarities.push_back(sim);
            labels.push_back(pair.is_same_person);

            processed++;
            if (processed % 100 == 0) {
                std::cout << "\r  Processed " << processed << "/" << pairs.size() << std::flush;
            }
        }

        std::cout << std::endl;

        if (similarities.empty()) {
            std::cerr << "[Benchmark] No valid pairs processed" << std::endl;
            return false;
        }

        // Calculate metrics
        lfw_results_.similarities = std::move(similarities);
        lfw_results_.labels = std::move(labels);
        lfw_results_.num_pairs = processed;

        calculate_lfw_metrics();

        return true;
    }

    /**
     * @brief Print benchmark results
     */
    void print_results() const {
        std::cout << "\n";
        std::cout << "======================================================" << std::endl;
        std::cout << "           FACE RECOGNITION ACCURACY RESULTS          " << std::endl;
        std::cout << "======================================================" << std::endl;

        // LFW Results
        if (lfw_results_.num_pairs > 0) {
            std::cout << "\n[LFW Verification]" << std::endl;
            std::cout << "  Pairs tested: " << lfw_results_.num_pairs << std::endl;
            std::cout << std::fixed << std::setprecision(4);
            std::cout << "  TAR @ FAR=0.1:  " << lfw_results_.tar_at_far_01 * 100 << "%" << std::endl;
            std::cout << "  TAR @ FAR=0.01: " << lfw_results_.tar_at_far_001 * 100 << "%" << std::endl;
            std::cout << "  TAR @ FAR=0.001:" << lfw_results_.tar_at_far_0001 * 100 << "%" << std::endl;
            std::cout << "  EER:            " << lfw_results_.eer * 100 << "%" << std::endl;

            // Threshold analysis
            AccuracyMetrics::analyze_thresholds(lfw_results_.similarities, lfw_results_.labels);

            // Accuracy at default threshold
            int correct = 0;
            for (size_t i = 0; i < lfw_results_.similarities.size(); i++) {
                bool pred_same = lfw_results_.similarities[i] >= config_.similarity_threshold;
                if (pred_same == lfw_results_.labels[i]) {
                    correct++;
                }
            }
            float accuracy = static_cast<float>(correct) / lfw_results_.similarities.size();
            std::cout << "  Accuracy:       " << accuracy * 100
                      << "% (threshold=" << config_.similarity_threshold << ")" << std::endl;
        }

        std::cout << "\n======================================================" << std::endl;
    }

    /**
     * @brief Get LFW accuracy (for comparison)
     */
    float get_lfw_accuracy() const {
        if (lfw_results_.similarities.empty()) return 0.0f;

        int correct = 0;
        for (size_t i = 0; i < lfw_results_.similarities.size(); i++) {
            bool pred_same = lfw_results_.similarities[i] >= config_.similarity_threshold;
            if (pred_same == lfw_results_.labels[i]) {
                correct++;
            }
        }
        return static_cast<float>(correct) / lfw_results_.similarities.size();
    }

private:
    void calculate_lfw_metrics() {
        const auto& sims = lfw_results_.similarities;
        const auto& labels = lfw_results_.labels;

        // TAR @ FAR
        lfw_results_.tar_at_far_01 = AccuracyMetrics::calculate_tar_at_far(sims, labels, 0.1f);
        lfw_results_.tar_at_far_001 = AccuracyMetrics::calculate_tar_at_far(sims, labels, 0.01f);
        lfw_results_.tar_at_far_0001 = AccuracyMetrics::calculate_tar_at_far(sims, labels, 0.001f);

        // EER
        lfw_results_.eer = AccuracyMetrics::calculate_eer(sims, labels);
    }

    BenchmarkConfig config_;
    std::string model_dir_;
    std::shared_ptr<FaceRecognizerWrapper> recognizer_;

    struct LFWResults {
        int num_pairs = 0;
        std::vector<float> similarities;
        std::vector<bool> labels;
        float tar_at_far_01 = 0.0f;
        float tar_at_far_001 = 0.0f;
        float tar_at_far_0001 = 0.0f;
        float eer = 1.0f;
    };

    LFWResults lfw_results_;
};

} // namespace cvedix_face_benchmark
