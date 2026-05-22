/**
 * @file cvedix_face_recognizer_node.cpp
 * @brief Implementation of dual-model face recognition node using SeetaFace6
 * 
 * Dual-model pipeline:
 *   detect → mask check → route to standard OR mask engines → match against correct DB
 * Registration:
 *   extract features with BOTH models → store in BOTH databases
 */

#ifdef CVEDIX_WITH_FACE

#include "cvedix_face_recognizer_node.h"
#include "cvedix_milvus_vector_search_node.h"

// SeetaFace6 headers
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/FaceDatabase.h>
#include <seeta/MaskDetector.h>
#include <seeta/Common/Struct.h>

#include <filesystem>
#include <fstream>

namespace cvedix_nodes {

    // ── Helper: zero-copy cv::Mat → SeetaImageData ──
    static SeetaImageData cvMatToSeetaImage(const cv::Mat& mat) {
        SeetaImageData simg;
        simg.width = mat.cols;
        simg.height = mat.rows;
        simg.channels = mat.channels();
        simg.data = mat.data;
        return simg;
    }

    // ── Helper: save name map to file ──
    static void saveNameMap(const std::string& path, const std::unordered_map<int64_t, std::string>& map) {
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            for (const auto& [id, name] : map) {
                ofs << id << " " << name << "\n";
            }
        }
    }

    // ── Helper: load name map from file ──
    static void loadNameMap(const std::string& path, std::unordered_map<int64_t, std::string>& map) {
        map.clear();
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            int64_t id;
            std::string name;
            while (ifs >> id >> name) {
                map[id] = name;
            }
        }
    }

    // ── Constructor ──
    cvedix_face_recognizer_node::cvedix_face_recognizer_node(
        std::string node_name,
        std::string model_dir,
        std::string db_path,
        float similarity_threshold,
        int min_face_size,
        bool use_68_landmarks,
        bool use_gpu,
        int gpu_id,
        FaceRecognizerMode mode,
        std::shared_ptr<cvedix_milvus_vector_search_node> milvus_node)
        : cvedix_primary_infer_node(node_name, ""),
          model_dir_(std::move(model_dir)),
          db_path_(std::move(db_path)),
          similarity_threshold_(similarity_threshold),
          min_face_size_(min_face_size),
          use_68_landmarks_(use_68_landmarks),
          use_gpu_(use_gpu),
          gpu_id_(gpu_id),
          mode_(mode),
          milvus_node_(milvus_node)
    {
        if (mode_ == FaceRecognizerMode::ASYNC) {
            worker_running_ = true;
            worker_ = std::thread(&cvedix_face_recognizer_node::workerLoop, this);
        }
        this->initialized();
    }

    // ── Destructor ──
    cvedix_face_recognizer_node::~cvedix_face_recognizer_node() {
        if (mode_ == FaceRecognizerMode::ASYNC) {
            worker_running_ = false;
            queue_cv_.notify_all();
            if (worker_.joinable()) worker_.join();
        }
        delete database_mask_;
        delete database_std_;
        delete recognizer_mask_;
        delete recognizer_std_;
        delete landmarker_mask_;
        delete landmarker_std_;
        delete mask_detector_;
        delete detector_;
        deinitialized();
    }

    // ── Initialize all engines (standard + mask) ──
    bool cvedix_face_recognizer_node::initEngines() {
        if (engines_initialized_) return true;

        try {
            if (!std::filesystem::exists(model_dir_)) {
                CVEDIX_WARN(cvedix_utils::string_format("SeetaFace6 model directory not found: %s", model_dir_.c_str()));
                return false;
            }

            auto modelPath = [this](const std::string& name) -> std::string {
                return model_dir_ + "/" + name;
            };

            // Determine device type
            auto device = use_gpu_ ? seeta::ModelSetting::GPU : seeta::ModelSetting::CPU;
            int  dev_id = use_gpu_ ? gpu_id_ : 0;
            std::string device_str = use_gpu_ ? "GPU:" + std::to_string(gpu_id_) : "CPU";

            // ──────── Shared: Face Detector ────────
            seeta::ModelSetting det_setting(modelPath("face_detector.csta"), device, dev_id);
            detector_ = new seeta::FaceDetector(det_setting);
            detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, min_face_size_);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] FaceDetector loaded [%s] (min_face=%d)", node_name.c_str(), device_str.c_str(), min_face_size_));

            // ──────── Shared: Mask Detector ────────
            seeta::ModelSetting md_setting(modelPath("mask_detector.csta"), device, dev_id);
            mask_detector_ = new seeta::MaskDetector(md_setting);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] MaskDetector loaded [%s]", node_name.c_str(), device_str.c_str()));

            // ──────── Standard pipeline (no mask) ────────
            std::string lm_std_model = use_68_landmarks_ ? "face_landmarker_pts68.csta" : "face_landmarker_pts5.csta";
            seeta::ModelSetting lm_std_setting(modelPath(lm_std_model), device, dev_id);
            landmarker_std_ = new seeta::FaceLandmarker(lm_std_setting);

            seeta::ModelSetting rec_std_setting(modelPath("face_recognizer.csta"), device, dev_id);
            recognizer_std_ = new seeta::FaceRecognizer(rec_std_setting);
            database_std_ = new seeta::FaceDatabase(rec_std_setting);

            CVEDIX_INFO(cvedix_utils::string_format("[%s] Standard pipeline loaded [%s] (%s + recognizer, feature_size=%d)",
                        node_name.c_str(), device_str.c_str(), lm_std_model.c_str(), recognizer_std_->GetExtractFeatureSize()));

            // ──────── Mask pipeline ────────
            seeta::ModelSetting lm_mask_setting(modelPath("face_landmarker_mask_pts5.csta"), device, dev_id);
            landmarker_mask_ = new seeta::FaceLandmarker(lm_mask_setting);

            seeta::ModelSetting rec_mask_setting(modelPath("face_recognizer_mask.csta"), device, dev_id);
            recognizer_mask_ = new seeta::FaceRecognizer(rec_mask_setting);
            database_mask_ = new seeta::FaceDatabase(rec_mask_setting);

            CVEDIX_INFO(cvedix_utils::string_format("[%s] Mask pipeline loaded [%s] (landmarker_mask_pts5 + recognizer_mask, feature_size=%d)",
                        node_name.c_str(), device_str.c_str(), recognizer_mask_->GetExtractFeatureSize()));

            engines_initialized_ = true;

            // ──────── Auto-load database if path specified ────────
            if (!db_path_.empty()) {
                std::string std_path = db_path_ + ".std";
                std::string mask_path = db_path_ + ".mask";
                bool has_std = std::filesystem::exists(std_path);
                bool has_mask = std::filesystem::exists(mask_path);

                if (has_std || has_mask) {
                    if (has_std && database_std_) {
                        database_std_->Load(std_path.c_str());
                        loadNameMap(std_path + ".names", id_to_name_std_);
                    }
                    if (has_mask && database_mask_) {
                        database_mask_->Load(mask_path.c_str());
                        loadNameMap(mask_path + ".names", id_to_name_mask_);
                    }
                    db_enabled_ = (database_std_ && database_std_->Count() > 0) ||
                                  (database_mask_ && database_mask_->Count() > 0);
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Auto-loaded database (std=%zu, mask=%zu faces)",
                                node_name.c_str(),
                                database_std_ ? database_std_->Count() : 0,
                                database_mask_ ? database_mask_->Count() : 0));
                } else {
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Database path set but no files found yet: %s",
                                node_name.c_str(), db_path_.c_str()));
                }
            }

            return true;

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to initialize SeetaFace6: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    // ──────────────────────────────────────────────────────────────
    //  Main pipeline: detect → mask check → route → extract → match
    // ──────────────────────────────────────────────────────────────
    void cvedix_face_recognizer_node::run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch)
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        if (!engines_initialized_ && !initEngines()) {
            return;
        }

        for (auto& frame_meta : frame_meta_with_batch) {
            if (frame_meta->frame.empty()) continue;

            cv::Mat frame = frame_meta->frame;
            SeetaImageData simg = cvMatToSeetaImage(frame);

            // Step 1: Detect all faces
            SeetaFaceInfoArray faces = detector_->detect(simg);

            for (int i = 0; i < faces.size; i++) {
                const auto& face = faces.data[i];
                const SeetaRect& rect = face.pos;

                // Validate bounding box
                int x = std::max(rect.x, 0);
                int y = std::max(rect.y, 0);
                int w = std::min(rect.width, frame.cols - x);
                int h = std::min(rect.height, frame.rows - y);
                if (w <= 0 || h <= 0) continue;

                // Step 2: Check if wearing mask
                float mask_score = 0.0f;
                bool wearing_mask = mask_detector_->detect(simg, rect, &mask_score);

                // Step 3: Route to appropriate engines
                seeta::FaceLandmarker* landmarker = wearing_mask ? landmarker_mask_ : landmarker_std_;
                seeta::FaceRecognizer* recognizer = wearing_mask ? recognizer_mask_ : recognizer_std_;
                seeta::FaceDatabase*   database   = wearing_mask ? database_mask_   : database_std_;
                auto& id_to_name                  = wearing_mask ? id_to_name_mask_ : id_to_name_std_;

                // Step 4: Detect landmarks (5 or 68 points)
                std::vector<SeetaPointF> points = landmarker->mark(simg, rect);

                // Store ALL points as keypoints for downstream nodes
                std::vector<std::pair<int, int>> keypoints;
                keypoints.reserve(points.size());
                for (const auto& pt : points) {
                    keypoints.emplace_back(static_cast<int>(pt.x), static_cast<int>(pt.y));
                }

                // FaceRecognizer::Extract() requires exactly 5 points (first 5)
                SeetaPointF* points_for_recognition = points.data();

                if (mode_ == FaceRecognizerMode::SYNC) {
                    // Step 5: Extract features
                    int feature_size = recognizer->GetExtractFeatureSize();
                    std::vector<float> features(feature_size);
                    bool extracted = recognizer->Extract(simg, points_for_recognition, features.data());

                    // Step 6: Database matching
                    std::string identity = "";
                    float identify_score = 0.0f;

#ifdef CVEDIX_WITH_MILVUS
                    if (milvus_node_ && extracted) {
                        auto results = milvus_node_->searchFace(features, 1);
                        if (!results.empty() && results[0].id >= 0) {
                            float sim = results[0].score; // Assuming Milvus Metric is IP (0.0 to 1.0)
                            if (sim >= similarity_threshold_) {
                                identity = results[0].name;
                                if (identity.empty()) identity = "ID_" + std::to_string(results[0].id);
                                identify_score = sim;
                            }
                        }
                    } else
#endif
                    if (db_enabled_ && database && database->Count() > 0 && extracted) {
                        float similarity = 0.0f;
                        int64_t idx = database->Query(simg, points_for_recognition, &similarity);

                        if (idx >= 0 && similarity >= similarity_threshold_) {
                            auto it = id_to_name.find(idx);
                            if (it != id_to_name.end()) {
                                identity = it->second;
                            } else {
                                identity = "ID_" + std::to_string(idx);
                            }
                            identify_score = similarity;
                        }
                    }

                    // Create face target
                    auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                        x, y, w, h, face.score, keypoints,
                        extracted ? features : std::vector<float>()
                    );

                    face_target->identify = identity;
                    face_target->identify_score = identify_score;

                    frame_meta->face_targets.push_back(face_target);
                } else {
                    // ASYNC MODE
                    int target_id = (x / 20) * 1000 + (y / 20) + 1000000;
                    std::string identity = "[SEARCHING...]";
                    float identify_score = 0.0f;
                    bool too_small = (w < 60 || h < 60);

                    {
                        std::lock_guard<std::mutex> lock(cache_mtx_);
                        if (identity_cache_.find(target_id) != identity_cache_.end()) {
                            identity = identity_cache_[target_id].label;
                            identify_score = identity_cache_[target_id].score;
                        } else if (!too_small) {
                            std::lock_guard<std::mutex> qlock(queue_mtx_);
                            if (pending_.find(target_id) == pending_.end()) {
                                pending_.insert(target_id);
                                std::vector<SeetaPointF> shifted_points;
                                for (size_t k = 0; k < 5 && k < points.size(); ++k) {
                                    SeetaPointF pt = points[k];
                                    pt.x -= x;
                                    pt.y -= y;
                                    shifted_points.push_back(pt);
                                }
                                search_queue_.push({target_id, wearing_mask, frame(cv::Rect(x, y, w, h)).clone(), shifted_points});
                                queue_cv_.notify_one();
                            }
                        } else {
                            identity = "[TOO SMALL]";
                        }

                        auto it = std::find(recent_faces_.begin(), recent_faces_.end(), target_id);
                        if (it != recent_faces_.end()) {
                            recent_faces_.erase(it);
                        }
                        recent_faces_.push_front(target_id);
                        if (recent_faces_.size() > (size_t)max_history_) {
                            recent_faces_.pop_back();
                        }
                    }

                    auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                        x, y, w, h, face.score, keypoints, std::vector<float>()
                    );
                    face_target->identify = identity;
                    face_target->identify_score = identify_score;
                    face_target->track_id = target_id;
                    frame_meta->face_targets.push_back(face_target);
                }
            }
        }

        if (mode_ == FaceRecognizerMode::ASYNC) {
            frame_count_++;
            if (frame_count_ % 300 == 0) {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                std::map<int, FaceIdentityResult> new_cache;
                for (int key : recent_faces_) {
                    if (identity_cache_.count(key)) {
                        new_cache[key] = identity_cache_[key];
                    }
                }
                identity_cache_ = std::move(new_cache);
            }
        }
    }

    // ──────────────────────────────────────────────────────────────
    //  Face Database API (dual-model)
    // ──────────────────────────────────────────────────────────────

    int64_t cvedix_face_recognizer_node::registerFace(const cv::Mat& image, const std::string& name) {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        if (!engines_initialized_ && !initEngines()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Cannot register face: engines not initialized", node_name.c_str()));
            return -1;
        }

        SeetaImageData simg = cvMatToSeetaImage(image);

        // Detect face
        SeetaFaceInfoArray faces = detector_->detect(simg);
        if (faces.size == 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] No face detected in registration image for '%s'", node_name.c_str(), name.c_str()));
            return -1;
        }

        // Use the largest face
        int best_idx = 0;
        int best_area = 0;
        for (int i = 0; i < faces.size; i++) {
            int area = faces.data[i].pos.width * faces.data[i].pos.height;
            if (area > best_area) {
                best_area = area;
                best_idx = i;
            }
        }
        const SeetaRect& best_face = faces.data[best_idx].pos;

        // ── Register with STANDARD model ──
        std::vector<SeetaPointF> points_std = landmarker_std_->mark(simg, best_face);
        int64_t idx_std = database_std_->Register(simg, points_std.data());
        if (idx_std >= 0) {
            id_to_name_std_[idx_std] = name;
        }

        // ── Register with MASK model ──
        // Mask model focuses on upper face (eyes, forehead) — works on unmasked photos too
        std::vector<SeetaPointF> points_mask = landmarker_mask_->mark(simg, best_face);
        int64_t idx_mask = database_mask_->Register(simg, points_mask.data());
        if (idx_mask >= 0) {
            id_to_name_mask_[idx_mask] = name;
        }

        // ── Register with Milvus (if connected) ──
        int64_t milvus_id = -1;
#ifdef CVEDIX_WITH_MILVUS
        if (milvus_node_) {
            std::vector<float> feat(recognizer_std_->GetExtractFeatureSize());
            if (recognizer_std_->Extract(simg, points_std.data(), feat.data())) {
                milvus_id = milvus_node_->registerFace(feat, name);
            }
        }
#endif

        if (idx_std >= 0 || idx_mask >= 0 || milvus_id >= 0) {
            db_enabled_ = true;
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Registered '%s' → std_id=%ld, mask_id=%ld, milvus_id=%ld",
                        node_name.c_str(), name.c_str(), idx_std, idx_mask, milvus_id));
        } else {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to register face '%s'", node_name.c_str(), name.c_str()));
        }

        return idx_std;
    }

    bool cvedix_face_recognizer_node::deleteFace(int64_t face_id_std, int64_t face_id_mask) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        bool deleted = false;

        // Delete from standard DB
        if (database_std_ && face_id_std >= 0) {
            if (database_std_->Delete(face_id_std) > 0) {
                // If mask id not provided, find by name then delete from mask DB
                if (face_id_mask < 0) {
                    auto it = id_to_name_std_.find(face_id_std);
                    if (it != id_to_name_std_.end()) {
                        const std::string& name = it->second;
                        // Find matching name in mask DB
                        for (auto& [mid, mname] : id_to_name_mask_) {
                            if (mname == name) {
                                face_id_mask = mid;
                                break;
                            }
                        }
                    }
                }
                id_to_name_std_.erase(face_id_std);
                deleted = true;
            }
        }

        // Delete from mask DB
        if (database_mask_ && face_id_mask >= 0) {
            if (database_mask_->Delete(face_id_mask) > 0) {
                id_to_name_mask_.erase(face_id_mask);
                deleted = true;
            }
        }

        if (deleted) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Deleted face std_id=%ld, mask_id=%ld",
                        node_name.c_str(), face_id_std, face_id_mask));
        }
        return deleted;
    }

    void cvedix_face_recognizer_node::clearDatabase() {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (database_std_) { database_std_->Clear(); id_to_name_std_.clear(); }
        if (database_mask_) { database_mask_->Clear(); id_to_name_mask_.clear(); }
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Both databases cleared", node_name.c_str()));
    }

    size_t cvedix_face_recognizer_node::getDatabaseSize() const {
        size_t count = 0;
        if (database_std_) count += database_std_->Count();
        return count;  // return standard count (should equal mask count)
    }

    bool cvedix_face_recognizer_node::saveDatabase(const std::string& path) {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        std::string base_path = path.empty() ? db_path_ : path;
        if (base_path.empty()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] No save path specified", node_name.c_str()));
            return false;
        }

        bool ok = true;

        // Save standard DB
        if (database_std_) {
            std::string std_path = base_path + ".std";
            ok &= database_std_->Save(std_path.c_str());
            saveNameMap(std_path + ".names", id_to_name_std_);
        }

        // Save mask DB
        if (database_mask_) {
            std::string mask_path = base_path + ".mask";
            ok &= database_mask_->Save(mask_path.c_str());
            saveNameMap(mask_path + ".names", id_to_name_mask_);
        }

        if (ok) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Databases saved to %s (.std=%zu, .mask=%zu faces)",
                        node_name.c_str(), base_path.c_str(),
                        database_std_ ? database_std_->Count() : 0,
                        database_mask_ ? database_mask_->Count() : 0));
        } else {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to save databases to %s", node_name.c_str(), base_path.c_str()));
        }
        return ok;
    }

    bool cvedix_face_recognizer_node::loadDatabase(const std::string& path) {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        if (!engines_initialized_ && !initEngines()) {
            return false;
        }

        bool ok = true;
        db_path_ = path;

        // Load standard DB
        if (database_std_) {
            std::string std_path = path + ".std";
            if (std::filesystem::exists(std_path)) {
                ok &= database_std_->Load(std_path.c_str());
                loadNameMap(std_path + ".names", id_to_name_std_);
            }
        }

        // Load mask DB
        if (database_mask_) {
            std::string mask_path = path + ".mask";
            if (std::filesystem::exists(mask_path)) {
                ok &= database_mask_->Load(mask_path.c_str());
                loadNameMap(mask_path + ".names", id_to_name_mask_);
            }
        }

        db_enabled_ = (database_std_ && database_std_->Count() > 0) || 
                       (database_mask_ && database_mask_->Count() > 0);

        CVEDIX_INFO(cvedix_utils::string_format("[%s] Databases loaded from %s (std=%zu, mask=%zu faces)",
                    node_name.c_str(), path.c_str(),
                    database_std_ ? database_std_->Count() : 0,
                    database_mask_ ? database_mask_->Count() : 0));
        return ok;
    }

    void cvedix_face_recognizer_node::setDatabaseEnabled(bool enabled) {
        db_enabled_ = enabled;
    }

    void cvedix_face_recognizer_node::setSimilarityThreshold(float threshold) {
        similarity_threshold_ = threshold;
    }

    void cvedix_face_recognizer_node::setMinFaceSize(int size) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        min_face_size_ = size;
        if (detector_) {
            detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, size);
        }
    }

    void cvedix_face_recognizer_node::workerLoop() {
        while (worker_running_) {
            AsyncSearchTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                queue_cv_.wait(lock, [&]{ return !search_queue_.empty() || !worker_running_; });
                if (!worker_running_) break;
                task = std::move(search_queue_.front());
                search_queue_.pop();
            }

            FaceIdentityResult info;
            
            while (worker_running_ && !engines_initialized_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (engines_initialized_ && !task.crop.empty()) {
                std::lock_guard<std::mutex> elock(engine_mutex_);

                SeetaImageData simg = cvMatToSeetaImage(task.crop);
                seeta::FaceRecognizer* recognizer = task.wearing_mask ? recognizer_mask_ : recognizer_std_;
                seeta::FaceDatabase*   database   = task.wearing_mask ? database_mask_   : database_std_;
                auto& id_to_name                  = task.wearing_mask ? id_to_name_mask_ : id_to_name_std_;

                int dim = recognizer->GetExtractFeatureSize();
                std::vector<float> feat(dim);

                if (recognizer->Extract(simg, task.points.data(), feat.data())) {
#ifdef CVEDIX_WITH_MILVUS
                    if (milvus_node_) {
                        auto results = milvus_node_->searchFace(feat, 1);
                        if (!results.empty() && results[0].id >= 0) {
                            float sim = results[0].score;
                            if (sim >= similarity_threshold_) {
                                std::string name = results[0].name;
                                if (name.empty()) name = "ID_" + std::to_string(results[0].id);
                                info.label = name;
                                info.score = sim;
                                info.resolved = true;
                            } else {
                                info.label = "Unknown";
                                info.resolved = true;
                            }
                        } else {
                            info.label = "Unknown";
                            info.resolved = true;
                        }
                    } else
#endif
                    if (db_enabled_ && database && database->Count() > 0) {
                        float similarity = 0.0f;
                        int64_t idx = database->Query(simg, task.points.data(), &similarity);

                        if (idx >= 0 && similarity >= similarity_threshold_) {
                            auto it = id_to_name.find(idx);
                            if (it != id_to_name.end()) {
                                info.label = it->second;
                            } else {
                                info.label = "ID_" + std::to_string(idx);
                            }
                            info.score = similarity;
                            info.resolved = true;
                        } else {
                            info.label = "Unknown";
                            info.resolved = true;
                        }
                    } else {
                        info.label = "Unknown";
                        info.resolved = true;
                    }
                } else {
                    info.label = "[EMBED FAIL]";
                    info.resolved = true;
                }
            }

            {
                std::lock_guard<std::mutex> lock(cache_mtx_);
                identity_cache_[task.target_id] = info;
            }
            {
                std::lock_guard<std::mutex> lock(queue_mtx_);
                pending_.erase(task.target_id);
            }
        }
    }

}

#endif // CVEDIX_WITH_FACE
