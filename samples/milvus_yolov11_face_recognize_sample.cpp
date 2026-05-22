/**
 * @file milvus_yolov11_face_recognize_sample.cpp
 * @brief YOLOv11 Face Detection + SeetaFace Embedding + Milvus Recognition
 *
 * Pipeline:
 *   file_src → yolov11_face → bridge (SeetaFace embeddings) → sort (FACE)
 *            → milvus_search → display_filter → osd → web_debug
 *
 * Features:
 *   - YOLOv11 for face detection (auto backend: TRT/ONNX/ORT)
 *   - SeetaFace6 for embedding extraction (1024-dim)
 *   - Milvus vector search to identify known faces (NO auto-registration)
 *   - SORT tracking for stable face IDs across frames
 *   - Displays identity + confidence score on OSD
 *   - Shows [IDENTIFIED] Name or [UNKNOWN] for each face
 *
 * This sample is the RECOGNITION counterpart to milvus_yolov11_face_register_sample.
 * Run the register sample first to populate the Milvus database, then run this sample
 * to identify faces from the database.
 *
 * Usage:
 *   ./milvus_yolov11_face_recognize_sample [video] [yolo_model] [seetaface_dir] [milvus_uri] [port]
 */

#if defined(CVEDIX_WITH_FACE) && defined(CVEDIX_WITH_MILVUS)

#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/infers/cvedix_milvus_vector_search_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/nodes/des/cvedix_fake_des_node.h"

// SeetaFace6 for embedding extraction (not detection)
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/PoseEstimator.h>
#include <seeta/AgePredictor.h>
#include <seeta/GenderPredictor.h>
#include <seeta/Common/Struct.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <map>
#include <set>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════
//  Helper
// ═══════════════════════════════════════════════════════════════
namespace {

std::string first_existing(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates)
        if (std::filesystem::exists(path)) return path;
    return candidates.empty() ? "" : candidates.front();
}

std::string ensure_face_labels_file() {
    const std::string p = "/tmp/cvedix_face_labels.txt";
    std::ofstream out(p, std::ios::trunc);
    if (out.is_open()) out << "face\n";
    return p;
}

static SeetaImageData cvMatToSeeta(const cv::Mat& mat) {
    SeetaImageData s;
    s.width = mat.cols;
    s.height = mat.rows;
    s.channels = mat.channels();
    s.data = mat.data;
    return s;
}

} // namespace

// ═══════════════════════════════════════════════════════════════
//  Bridge Node: YOLO targets → face_targets with SeetaFace embeddings
//  (Same as register sample — converts YOLO detection to face embeddings)
// ═══════════════════════════════════════════════════════════════
class cvedix_yolo_face_bridge_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_yolo_face_bridge_node(std::string name, const std::string& model_dir)
        : cvedix_node(name), model_dir_(model_dir) {
        this->initialized();
    }

    ~cvedix_yolo_face_bridge_node() {
        delete recognizer_;
        delete landmarker_;
        deinitialized();
    }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initEngines()) return meta;
        if (meta->targets.empty() || meta->frame.empty()) return meta;

        SeetaImageData simg = cvMatToSeeta(meta->frame);

        for (auto& target : meta->targets) {
            SeetaRect rect;
            rect.x = std::max(0, target->x);
            rect.y = std::max(0, target->y);
            rect.width  = std::min(target->width,  meta->frame.cols - rect.x);
            rect.height = std::min(target->height, meta->frame.rows - rect.y);
            if (rect.width <= 10 || rect.height <= 10) continue;

            // Landmarks (5-point)
            auto points = landmarker_->mark(simg, rect);
            std::vector<std::pair<int,int>> kps;
            kps.reserve(points.size());
            for (auto& pt : points)
                kps.emplace_back(static_cast<int>(pt.x), static_cast<int>(pt.y));

            // Extract embedding (1024-dim)
            int dim = recognizer_->GetExtractFeatureSize();
            std::vector<float> feat(dim);
            if (!recognizer_->Extract(simg, points.data(), feat.data())) continue;

            auto ft = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                rect.x, rect.y, rect.width, rect.height,
                target->primary_score, kps, feat);

            meta->face_targets.push_back(ft);
        }

        // Clear normal targets so OSD only renders face_targets
        meta->targets.clear();
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }

private:
    bool initEngines() {
        if (ready_) return true;
        try {
            auto mp = [&](const std::string& n){ return model_dir_ + "/" + n; };
            landmarker_ = new seeta::FaceLandmarker(seeta::ModelSetting(mp("face_landmarker_pts5.csta")));
            recognizer_ = new seeta::FaceRecognizer(seeta::ModelSetting(mp("face_recognizer.csta")));
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] SeetaFace6 bridge loaded (dim=%d)", node_name.c_str(), recognizer_->GetExtractFeatureSize()));
            ready_ = true;
            return true;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] SeetaFace6 init failed: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    std::string model_dir_;
    seeta::FaceLandmarker* landmarker_ = nullptr;
    seeta::FaceRecognizer* recognizer_ = nullptr;
    bool ready_ = false;
    std::mutex mtx_;
};

// ═══════════════════════════════════════════════════════════════
//  Async Face Recognize Node
//
//  Performance optimization: cache identity by track_id.
//  Only query Milvus when a NEW track_id appears.
//  Background thread processes Milvus queries → no pipeline blocking.
// ═══════════════════════════════════════════════════════════════
class cvedix_async_face_recognize_node : public cvedix_nodes::cvedix_node {
public:
    struct IdentityInfo {
        std::string label;
        float score = 0.0f;
        int64_t milvus_id = -1;
    };

    cvedix_async_face_recognize_node(
        std::string name,
        std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus,
        float max_l2_distance = 1.2f)
        : cvedix_node(name), milvus_(milvus), max_l2_dist_(max_l2_distance)
    {
        // Start background worker
        worker_running_ = true;
        worker_ = std::thread(&cvedix_async_face_recognize_node::workerLoop, this);
        this->initialized();
    }

    ~cvedix_async_face_recognize_node() {
        worker_running_ = false;
        queue_cv_.notify_all();
        if (worker_.joinable()) worker_.join();
        deinitialized();
    }

    virtual bool is_async() const override { return true; }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override
    {
        if (meta->face_targets.empty()) return meta;

        for (auto& face : meta->face_targets) {
            int tid = face->track_id;
            if (tid < 0 || face->embeddings.empty()) continue;

            // Check cache (lock-free read for hot path)
            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                auto it = identity_cache_.find(tid);
                if (it != identity_cache_.end()) {
                    // Cache hit → instant, no Milvus call
                    face->identify = it->second.label;
                    face->identify_score = it->second.score;
                    continue;
                }
            }

            // Cache miss → queue for background Milvus search
            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                if (pending_.find(tid) == pending_.end()) {
                    pending_.insert(tid);
                    search_queue_.push({tid, face->embeddings});
                    queue_cv_.notify_one();
                }
            }

            // Show "searching..." while waiting
            face->identify = "[SEARCHING...] #" + std::to_string(tid);
            face->identify_score = 0.0f;
        }
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }

private:
    struct SearchTask {
        int track_id;
        std::vector<float> embedding;
    };

    void workerLoop() {
        while (worker_running_) {
            SearchTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                queue_cv_.wait(lock, [&]{ return !search_queue_.empty() || !worker_running_; });
                if (!worker_running_) break;
                task = std::move(search_queue_.front());
                search_queue_.pop();
            }

            // Perform Milvus search (blocking, but in background thread)
            IdentityInfo info;
            if (milvus_) {
                auto results = milvus_->searchFace(task.embedding, 1);
                if (!results.empty() && results[0].id >= 0) {
                    float l2 = results[0].score;
                    float sim = std::max(0.0f, 1.0f - (l2 * l2) / 2.0f);

                    if (l2 <= max_l2_dist_) {
                        std::string name = results[0].name;
                        if (name.empty()) name = "ID-" + std::to_string(results[0].id);
                        int pct = static_cast<int>(sim * 100.0f);
                        info.label = "[IDENTIFIED] " + name + " (" + std::to_string(pct) + "%)";
                        info.score = sim;
                        info.milvus_id = results[0].id;

                        CVEDIX_INFO(cvedix_utils::string_format(
                            "[%s] Identified track#%d → %s (L2=%.3f, sim=%.0f%%)",
                            node_name.c_str(), task.track_id, name.c_str(), l2, sim * 100));
                    } else {
                        info.label = "[UNKNOWN]";
                    }
                } else {
                    info.label = "[UNKNOWN]";
                }
            }

            // Update cache
            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                info.label += " #" + std::to_string(task.track_id);
                identity_cache_[task.track_id] = info;
            }
            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                pending_.erase(task.track_id);
            }
        }
    }

    std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus_;
    float max_l2_dist_;

    // Identity cache: track_id → identity (read from pipeline, written by worker)
    std::map<int, IdentityInfo> identity_cache_;
    std::mutex cache_mtx_;

    // Search queue
    std::queue<SearchTask> search_queue_;
    std::set<int> pending_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;

    // Background worker
    std::thread worker_;
    std::atomic<bool> worker_running_{false};
};

// ═══════════════════════════════════════════════════════════════
//  Face Crop Debug + Async Recognition (All-in-One)
//
//  This node does everything in one place:
//  1. Takes YOLO frame_targets (face bboxes)
//  2. Crops faces and renders them in a panel below the video
//  3. Background thread: SeetaFace embedding → Milvus search
//  4. Shows identity labels on the crop panel
//  5. Pipeline is NOT blocked — recognition is fully async
// ═══════════════════════════════════════════════════════════════
class cvedix_face_crop_debug_node : public cvedix_nodes::cvedix_node {
public:
    struct FaceIdentity {
        std::string label = "[SEARCHING...]";
        float score = 0.0f;
        bool resolved = false;
        cv::Mat crop; // Store crop for API
    };

    cvedix_face_crop_debug_node(
        std::string name,
        const std::string& seetaface_dir,
        std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus,
        int crop_size = 80, int panel_height = 130, float max_l2 = 1.2f)
        : cvedix_node(name), model_dir_(seetaface_dir), milvus_(milvus),
          crop_size_(crop_size), panel_h_(panel_height), max_l2_dist_(max_l2)
    {
        worker_running_ = true;
        worker_ = std::thread(&cvedix_face_crop_debug_node::workerLoop, this);
        this->max_in_queue_size = 50; // Increase queue size to prevent dropping
        this->initialized();
    }

    ~cvedix_face_crop_debug_node() {
        worker_running_ = false;
        queue_cv_.notify_all();
        if (worker_.joinable()) worker_.join();
        delete recognizer_;
        delete landmarker_;
        delete pose_estimator_;
        deinitialized();
    }

    virtual bool is_async() const override { return true; }

    // Register HTTP API routes for the dashboard
    void register_api(httplib::Server& server) {
        server.Get("/api/faces", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(cache_mtx_);
            std::string json = "[";
            bool first = true;
            for (int key : recent_faces_) {
                auto it = identity_cache_.find(key);
                if (it == identity_cache_.end()) continue;
                auto const& info = it->second;
                if (!first) json += ",";
                json += "{\"id\":" + std::to_string(key) +
                        ",\"label\":\"" + info.label + "\"" +
                        ",\"score\":" + std::to_string(info.score) + "}";
                first = false;
            }
            json += "]";
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(json, "application/json");
        });

        server.Get(R"(/snapshot/face/([0-9]+))", [this](const httplib::Request& req, httplib::Response& res) {
            int key = std::stoi(req.matches[1]);
            cv::Mat crop;
            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                auto it = identity_cache_.find(key);
                if (it != identity_cache_.end()) crop = it->second.crop.clone();
            }
            if (!crop.empty()) {
                std::vector<uint8_t> buf;
                cv::imencode(".jpg", crop, buf, {cv::IMWRITE_JPEG_QUALITY, 85});
                res.set_header("Cache-Control", "no-cache, no-store");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(std::string(reinterpret_cast<const char*>(buf.data()), buf.size()), "image/jpeg");
            } else {
                res.status = 204;
            }
        });
    }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override
    {
        if (meta->frame.empty()) return meta;
        auto& frame = meta->frame;
        int fw = frame.cols;

        // Create dark panel below main frame
        cv::Mat panel(panel_h_, fw, frame.type(), cv::Scalar(20, 20, 20));
        int x_offset = 10;
        int max_faces_panel = std::min((fw - 20) / (crop_size_ + 10), max_history_);
        int count = 0;

        for (auto& target : meta->targets) {
            int fx = std::max(0, target->x);
            int fy = std::max(0, target->y);
            int fww = std::min(target->width, frame.cols - fx);
            int fhh = std::min(target->height, frame.rows - fy);
            // Basic sanity check to prevent cv::resize crash
            if (fww < 30 || fhh < 30) continue;

            cv::Mat crop = frame(cv::Rect(fx, fy, fww, fhh));
            cv::Mat resized;
            cv::resize(crop, resized, cv::Size(crop_size_, crop_size_));

            // Use track_id for identity caching if available, otherwise fallback to spatial hash
            int bbox_key = target->track_id >= 0 ? target->track_id : ((fx / 20) * 1000 + (fy / 20) + 1000000);

            // Check if face is large enough for recognition
            bool too_small = (fww < 30 || fhh < 30);

            // Queue for async recognition if not already resolved
            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                if (identity_cache_.find(bbox_key) == identity_cache_.end()) {
                    if (too_small) {
                        identity_cache_[bbox_key].label = "[TOO SMALL]";
                        identity_cache_[bbox_key].crop = resized.clone();
                        identity_cache_[bbox_key].resolved = true;
                    } else {
                        std::lock_guard<std::mutex> qlock(queue_mtx_);
                        if (pending_.find(bbox_key) == pending_.end()) {
                            pending_.insert(bbox_key);
                            search_queue_.push({bbox_key, crop.clone()});
                            queue_cv_.notify_one();
                        }
                    }
                }
                // Update crop and history (already under cache_mtx_ lock)
                identity_cache_[bbox_key].crop = resized.clone();

                auto it = std::find(recent_faces_.begin(), recent_faces_.end(), bbox_key);
                if (it != recent_faces_.end()) {
                    recent_faces_.erase(it);
                }
                recent_faces_.push_front(bbox_key);
                if (recent_faces_.size() > (size_t)max_history_) {
                    recent_faces_.pop_back();
                }
            }
        }

        // Place recent crops on panel
        {
            std::lock_guard<std::mutex> lock(cache_mtx_);
            for (int key : recent_faces_) {
                if (count >= max_faces_panel) break;

                auto it = identity_cache_.find(key);
                if (it == identity_cache_.end() || it->second.crop.empty()) continue;

                cv::Mat resized = it->second.crop;
                int y_offset = 5;
                resized.copyTo(panel(cv::Rect(x_offset, y_offset, crop_size_, crop_size_)));

                // Draw border (green for detection)
                cv::rectangle(panel, cv::Rect(x_offset, y_offset, crop_size_, crop_size_),
                             cv::Scalar(0, 255, 0), 2);

                // Label text: Just detection label, not recognition
                std::string disp = "face " + std::to_string(key);
                cv::putText(panel, disp, cv::Point(x_offset, y_offset + crop_size_ + 14),
                            cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(200, 200, 200), 1);

                x_offset += crop_size_ + 10;
                count++;
            }
        }

        // Draw separator
        cv::line(panel, cv::Point(0, 0), cv::Point(fw, 0), cv::Scalar(80, 80, 80), 1);

        // Append panel below main frame
        cv::Mat combined;
        cv::vconcat(frame, panel, combined);
        meta->frame = combined;

        // Clean old cache entries periodically
        frame_count_++;
        if (frame_count_ % 300 == 0) {
            std::lock_guard<std::mutex> lock(cache_mtx_);
            // Instead of clearing completely, keep the ones in recent history
            std::map<int, FaceIdentity> new_cache;
            for (int key : recent_faces_) {
                if (identity_cache_.count(key)) {
                    new_cache[key] = identity_cache_[key];
                }
            }
            identity_cache_ = std::move(new_cache);
        }

        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }

private:
    struct SearchTask {
        int bbox_key;
        cv::Mat face_crop;
    };

    bool initSeetaFace() {
        if (seeta_ready_) return true;
        try {
            auto mp = [&](const std::string& n){ return model_dir_ + "/" + n; };
            landmarker_ = new seeta::FaceLandmarker(seeta::ModelSetting(mp("face_landmarker_pts5.csta")));
            recognizer_ = new seeta::FaceRecognizer(seeta::ModelSetting(mp("face_recognizer.csta")));
            pose_estimator_ = new seeta::PoseEstimator(seeta::ModelSetting(mp("pose_estimation.csta")));
            age_predictor_ = new seeta::AgePredictor(seeta::ModelSetting(mp("age_predictor.csta")));
            gender_predictor_ = new seeta::GenderPredictor(seeta::ModelSetting(mp("gender_predictor.csta")));
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] SeetaFace6 loaded (dim=%d, pose=enabled, age=enabled, gender=enabled)", node_name.c_str(), recognizer_->GetExtractFeatureSize()));
            seeta_ready_ = true;
            return true;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] SeetaFace6 init failed: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    void workerLoop() {
        while (worker_running_) {
            SearchTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                queue_cv_.wait(lock, [&]{ return !search_queue_.empty() || !worker_running_; });
                if (!worker_running_) break;
                task = std::move(search_queue_.front());
                search_queue_.pop();
            }

            FaceIdentity info;
            if (initSeetaFace() && !task.face_crop.empty()) {
                // Extract embedding from face crop
                SeetaImageData simg = cvMatToSeeta(task.face_crop);
                SeetaRect rect{0, 0, task.face_crop.cols, task.face_crop.rows};

                // Estimate Pose
                float yaw = 0, pitch = 0, roll = 0;
                pose_estimator_->Estimate(simg, rect, &yaw, &pitch, &roll);

                if (std::abs(yaw) > 15.0f || std::abs(pitch) > 15.0f) {
                    info.label = "[BAD POSE]";
                    info.resolved = true;
                } else {
                    auto points = landmarker_->mark(simg, rect);
                    
                    int age = -1;
                    seeta::GenderPredictor::GENDER gender;
                    bool has_age = age_predictor_->PredictAgeWithCrop(simg, points.data(), age);
                    bool has_gender = gender_predictor_->PredictGenderWithCrop(simg, points.data(), gender);
                    
                    std::string attr_str = "";
                    if (has_gender && has_age) {
                        attr_str = (gender == seeta::GenderPredictor::MALE ? "Nam" : "Nu") + std::string(", ") + std::to_string(age) + "t";
                    } else if (has_age) {
                        attr_str = std::to_string(age) + "t";
                    }

                    int dim = recognizer_->GetExtractFeatureSize();
                    std::vector<float> feat(dim);

                    if (recognizer_->Extract(simg, points.data(), feat.data())) {
                        // Search Milvus
                        if (milvus_) {
                            auto results = milvus_->searchFace(feat, 1);
                            if (!results.empty() && results[0].id >= 0) {
                                float l2 = results[0].score;
                                float sim = std::max(0.0f, 1.0f - (l2 * l2) / 2.0f);

                                if (l2 <= max_l2_dist_) {
                                    std::string name = results[0].name;
                                    if (name.empty()) name = "ID-" + std::to_string(results[0].id);
                                    int pct = static_cast<int>(sim * 100.0f);
                                    info.label = name;
                                    if (!attr_str.empty()) info.label += " (" + attr_str + ")";
                                    info.label += " - " + std::to_string(pct) + "%";
                                    info.score = sim;
                                    info.resolved = true;
                                } else {
                                    info.label = "Unknown";
                                    if (!attr_str.empty()) info.label += " (" + attr_str + ")";
                                    info.resolved = true;
                                }
                            } else {
                                info.label = "Unknown";
                                info.resolved = true;
                            }
                        }
                    } else {
                        info.label = "[EMBED FAIL]";
                        info.resolved = true;
                    }
                }
            }

            // Update cache while preserving crop
            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                auto it = identity_cache_.find(task.bbox_key);
                if (it != identity_cache_.end()) {
                    info.crop = it->second.crop; // Preserve the latest crop
                }
                identity_cache_[task.bbox_key] = info;
            }
            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                pending_.erase(task.bbox_key);
            }
        }
    }

    std::string model_dir_;
    std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus_;
    int crop_size_, panel_h_;
    float max_l2_dist_;
    int frame_count_ = 0;
    std::deque<int> recent_faces_;
    int max_history_ = 10;

    // SeetaFace6
    seeta::FaceLandmarker* landmarker_ = nullptr;
    seeta::FaceRecognizer* recognizer_ = nullptr;
    seeta::PoseEstimator* pose_estimator_ = nullptr;
    seeta::AgePredictor* age_predictor_ = nullptr;
    seeta::GenderPredictor* gender_predictor_ = nullptr;
    bool seeta_ready_ = false;

    // Identity cache: bbox_key → identity
    std::map<int, FaceIdentity> identity_cache_;
    std::mutex cache_mtx_;

    // Search queue
    std::queue<SearchTask> search_queue_;
    std::set<int> pending_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;

    // Background worker
    std::thread worker_;
    std::atomic<bool> worker_running_{false};
};

// ═══════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
    // Limit OpenMP threads to prevent SeetaFace async worker from starving the main pipeline
    setenv("OMP_NUM_THREADS", "2", 1);
    
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string video_path = first_existing({
        "../../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4"
    });
    std::string face_model = first_existing({
        "../../cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "./cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "../cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "../../cvedix_data/models/yolov11-model-face.onnx",
        "./cvedix_data/models/yolov11-model-face.onnx",
        "../cvedix_data/models/yolov11-model-face.onnx",
        "../../cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "./cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "../cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml"
    });
    std::string seetaface_dir = first_existing({
        "../../cvedix_data/models/seetaface6/sf3.0_models",
        "../cvedix_data/models/seetaface6/sf3.0_models",
        "./cvedix_data/models/seetaface6/sf3.0_models"
    });
    std::string milvus_uri  = "localhost:19530";
    std::string collection  = "cvedix_faces_yolov11";   // Same collection as register sample
    int port = 9096;  // Different port from register sample (9095)

    if (argc > 1) video_path    = argv[1];
    if (argc > 2) face_model    = argv[2];
    if (argc > 3) seetaface_dir = argv[3];
    if (argc > 4) milvus_uri    = argv[4];
    if (argc > 5) port          = std::stoi(argv[5]);

    CVEDIX_INFO("═══════════════════════════════════════════════════════════");
    CVEDIX_INFO("  YOLOv11 Face Recognition + Milvus Search               ");
    CVEDIX_INFO("═══════════════════════════════════════════════════════════");
    CVEDIX_INFO("Video:      " + video_path);
    CVEDIX_INFO("Face Model: " + face_model);
    CVEDIX_INFO("SeetaFace:  " + seetaface_dir);
    CVEDIX_INFO("Milvus:     " + milvus_uri);
    CVEDIX_INFO("Collection: " + collection);

    const std::string labels = ensure_face_labels_file();

    // ── Pipeline Nodes ──

    // 1. Source (loop video)
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    // 2. YOLOv11 Face Detector (multi-backend fallback)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolov11_face");
    bool det_loaded = detector->load_model(
        face_model, cvedix_nodes::YoloVersion::YOLO11, labels,
        0.65f, 0.45f, 0, cvedix_nodes::BackendType::AUTO);

    // Fallback: try ONNX model with ORT backend
    if (!det_loaded) {
        std::string onnx = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"});
        if (std::filesystem::exists(onnx)) {
            CVEDIX_INFO("Fallback: trying ORT backend with " + onnx);
            det_loaded = detector->load_model(
                onnx, cvedix_nodes::YoloVersion::YOLO11, labels,
                0.65f, 0.45f, 0, cvedix_nodes::BackendType::ORT);
        }
    }
    // Fallback: try ONNX model with OpenCV DNN backend
    if (!det_loaded) {
        std::string onnx = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"});
        if (std::filesystem::exists(onnx)) {
            CVEDIX_INFO("Fallback: trying ONNX (OpenCV DNN) backend with " + onnx);
            det_loaded = detector->load_model(
                onnx, cvedix_nodes::YoloVersion::YOLO11, labels,
                0.65f, 0.45f, 0, cvedix_nodes::BackendType::ONNX);
        }
    }
    if (!det_loaded) {
        CVEDIX_ERROR("Failed to load YOLOv11 face model. Tried all backends.");
        return 1;
    }
    detector->set_allowed_classes({0}); // class 0 = face

    // 3. SORT Tracker (assign IDs to faces)
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("tracker");

    // 4. Crop Debug + Async Recognition
    //    This node handles BOTH display (face crop panel) AND recognition
    //    (async SeetaFace embedding + Milvus search in background thread).
    //    Uses YOLO frame_targets directly — no separate bridge needed.
    auto milvus = std::make_shared<cvedix_nodes::cvedix_milvus_vector_search_node>(
        "milvus_search", milvus_uri, collection, 1024, 0.0f);
    auto crop_debug = std::make_shared<cvedix_face_crop_debug_node>(
        "crop_debug", seetaface_dir, milvus, 80, 130, 1.2f);

    // 5. OSD (shows YOLO face detection boxes — fast, no lag)
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = false;
    osd_cfg.show_track_trail = false;
    osd_cfg.show_center_dot = true;
    osd_cfg.enable_face = false;  // Display uses frame_targets (YOLO boxes)
    osd_cfg.label_color  = cv::Scalar(0, 255, 100);
    osd_cfg.bbox_color   = cv::Scalar(0, 255, 100);
    osd_cfg.bbox_thickness = 2;
    osd->update_config(osd_cfg);

    // 5. Web Debug
    auto web = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 50);

    // ═══════════════════════════════════════════════════════════════
    //  Single Linear Pipeline (no branching)
    //
    //  file_src → yolo → sort → crop_debug → osd → web_debug
    //                       ↑ async SeetaFace + Milvus (background thread)
    //                       ↑ face crop panel appended below video
    // ═══════════════════════════════════════════════════════════════
    auto milvus_des = std::make_shared<cvedix_nodes::cvedix_fake_des_node>("milvus_des", 0);

    detector->attach_to({file_src});
    tracker->attach_to({detector});
    crop_debug->attach_to({tracker});
    
    // Milvus branch
    milvus->attach_to({crop_debug}); 
    milvus_des->attach_to({milvus}); // Close the milvus branch with a DES node
    
    // Web display branch
    osd->attach_to({crop_debug});
    web->attach_to({osd});

    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web->set_board(&board);

    // Register HTTP APIs for the new dashboard Result panel
    crop_debug->register_api(web->get_server());

    // ── Start ──
    file_src->start();

    std::cout << "\n"
        << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║  YOLOv11 Face Recognition (Single Pipeline)                ║\n"
        << "║                                                            ║\n"
        << "║  Pipeline:                                                 ║\n"
        << "║    file_src → yolo → sort → crop_debug(async) → osd → web_debug  ║\n"
        << "║                                                            ║\n"
        << "║  Features:                                                 ║\n"
        << "║    • Display: YOLO face detection (fast, no lag)           ║\n"
        << "║    • Crop panel: face thumbnails + identity below video    ║\n"
        << "║    • Tracking: SORT tracking prevents repeated recognition ║\n"
        << "║    • Recognition: async SeetaFace + Milvus (background)   ║\n"
        << "║                                                            ║\n"
        << "║  Open: http://localhost:" << port << "                                 ║\n"
        << "║  Press Enter to stop...                                    ║\n"
        << "╚══════════════════════════════════════════════════════════════╝\n"
        << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    file_src->detach_recursively();
    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "Requires CVEDIX_WITH_FACE=ON and CVEDIX_WITH_MILVUS=ON\n";
    return 1;
}
#endif
