/**
 * @file cvedix_face_analysis_node.cpp
 * @brief Implementation of all-in-one face analysis node using SeetaFace6
 */

#ifdef CVEDIX_WITH_FACE

#include "cvedix_face_analysis_node.h"

#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/FaceDatabase.h>
#include <seeta/MaskDetector.h>
#include <seeta/AgePredictor.h>
#include <seeta/GenderPredictor.h>
#include <seeta/EyeStateDetector.h>
#include <seeta/PoseEstimator.h>
#include <seeta/FaceAntiSpoofing.h>
#include <seeta/Common/Struct.h>

#include <filesystem>
#include <fstream>

namespace cvedix_nodes {

    // ── Helper: zero-copy cv::Mat → SeetaImageData ──
    static SeetaImageData cvMatToSeeta(const cv::Mat& mat) {
        SeetaImageData simg;
        simg.width = mat.cols;
        simg.height = mat.rows;
        simg.channels = mat.channels();
        simg.data = mat.data;
        return simg;
    }

    static void saveNameMap(const std::string& path, const std::unordered_map<int64_t, std::string>& map) {
        std::ofstream ofs(path);
        if (ofs.is_open()) {
            for (const auto& [id, name] : map) ofs << id << " " << name << "\n";
        }
    }

    static void loadNameMap(const std::string& path, std::unordered_map<int64_t, std::string>& map) {
        map.clear();
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            int64_t id; std::string name;
            while (ifs >> id >> name) map[id] = name;
        }
    }

    // ── Constructor ──
    cvedix_face_analysis_node::cvedix_face_analysis_node(
        std::string node_name, std::string model_dir, std::string db_path,
        float similarity_threshold, int min_face_size,
        bool enable_anti_spoofing, bool enable_age_gender,
        bool enable_eye_state, bool enable_pose,
        bool use_gpu, int gpu_id)
        : cvedix_primary_infer_node(node_name, ""),
          model_dir_(std::move(model_dir)),
          db_path_(std::move(db_path)),
          similarity_threshold_(similarity_threshold),
          min_face_size_(min_face_size),
          enable_anti_spoofing_(enable_anti_spoofing),
          enable_age_gender_(enable_age_gender),
          enable_eye_state_(enable_eye_state),
          enable_pose_(enable_pose),
          use_gpu_(use_gpu),
          gpu_id_(gpu_id)
    {
        this->initialized();
    }

    // ── Destructor ──
    cvedix_face_analysis_node::~cvedix_face_analysis_node() {
        delete database_mask_;
        delete database_std_;
        delete recognizer_mask_;
        delete recognizer_std_;
        delete landmarker_mask_;
        delete anti_spoofing_;
        delete pose_estimator_;
        delete eye_detector_;
        delete gender_predictor_;
        delete age_predictor_;
        delete mask_detector_;
        delete landmarker_;
        delete detector_;
        deinitialized();
    }

    // ── Initialize all engines ──
    bool cvedix_face_analysis_node::initEngines() {
        if (engines_initialized_) return true;

        try {
            if (!std::filesystem::exists(model_dir_)) {
                CVEDIX_WARN(cvedix_utils::string_format("SeetaFace6 model directory not found: %s", model_dir_.c_str()));
                return false;
            }

            auto modelPath = [this](const std::string& name) -> std::string {
                return model_dir_ + "/" + name;
            };

            auto device = use_gpu_ ? seeta::ModelSetting::GPU : seeta::ModelSetting::CPU;
            int dev_id = use_gpu_ ? gpu_id_ : 0;
            std::string device_str = use_gpu_ ? "GPU:" + std::to_string(gpu_id_) : "CPU";

            // ──────── Face Detector ────────
            seeta::ModelSetting det_setting(modelPath("face_detector.csta"), device, dev_id);
            detector_ = new seeta::FaceDetector(det_setting);
            detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, min_face_size_);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] FaceDetector loaded [%s] (min_face=%d)", node_name.c_str(), device_str.c_str(), min_face_size_));

            // ──────── Mask Detector ────────
            seeta::ModelSetting md_setting(modelPath("mask_detector.csta"), device, dev_id);
            mask_detector_ = new seeta::MaskDetector(md_setting);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] MaskDetector loaded", node_name.c_str()));

            // ──────── Landmarker (5-point, standard) ────────
            seeta::ModelSetting lm_setting(modelPath("face_landmarker_pts5.csta"), device, dev_id);
            landmarker_ = new seeta::FaceLandmarker(lm_setting);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] FaceLandmarker (pts5) loaded", node_name.c_str()));

            // ──────── Age Predictor ────────
            if (enable_age_gender_) {
                seeta::ModelSetting age_setting(modelPath("age_predictor.csta"), device, dev_id);
                age_predictor_ = new seeta::AgePredictor(age_setting);
                CVEDIX_INFO(cvedix_utils::string_format("[%s] AgePredictor loaded", node_name.c_str()));

                seeta::ModelSetting gender_setting(modelPath("gender_predictor.csta"), device, dev_id);
                gender_predictor_ = new seeta::GenderPredictor(gender_setting);
                CVEDIX_INFO(cvedix_utils::string_format("[%s] GenderPredictor loaded", node_name.c_str()));
            }

            // ──────── Eye State Detector ────────
            if (enable_eye_state_) {
                seeta::ModelSetting eye_setting(modelPath("eye_state.csta"), device, dev_id);
                eye_detector_ = new seeta::EyeStateDetector(eye_setting);
                CVEDIX_INFO(cvedix_utils::string_format("[%s] EyeStateDetector loaded", node_name.c_str()));
            }

            // ──────── Pose Estimator ────────
            if (enable_pose_) {
                seeta::ModelSetting pose_setting(modelPath("pose_estimation.csta"), device, dev_id);
                pose_estimator_ = new seeta::PoseEstimator(pose_setting);
                CVEDIX_INFO(cvedix_utils::string_format("[%s] PoseEstimator loaded", node_name.c_str()));
            }

            // ──────── Anti-Spoofing ────────
            if (enable_anti_spoofing_) {
                std::string fas1 = modelPath("fas_first.csta");
                std::string fas2 = modelPath("fas_second.csta");
                if (std::filesystem::exists(fas1) && std::filesystem::exists(fas2)) {
                    seeta::ModelSetting fas_setting;
                    fas_setting.append(fas1);
                    fas_setting.append(fas2);
                    fas_setting.set_device(device);
                    fas_setting.set_id(dev_id);
                    anti_spoofing_ = new seeta::FaceAntiSpoofing(fas_setting);
                    anti_spoofing_->SetThreshold(0.3f, 0.80f);
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] FaceAntiSpoofing loaded (dual model)", node_name.c_str()));
                } else if (std::filesystem::exists(fas1)) {
                    seeta::ModelSetting fas_setting(fas1, device, dev_id);
                    anti_spoofing_ = new seeta::FaceAntiSpoofing(fas_setting);
                    anti_spoofing_->SetThreshold(0.3f, 0.80f);
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] FaceAntiSpoofing loaded (single model)", node_name.c_str()));
                } else {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] Anti-spoofing models not found, disabled", node_name.c_str()));
                }
            }

            // ──────── Recognition (standard + mask) ────────
            seeta::ModelSetting rec_std_setting(modelPath("face_recognizer.csta"), device, dev_id);
            recognizer_std_ = new seeta::FaceRecognizer(rec_std_setting);
            database_std_ = new seeta::FaceDatabase(rec_std_setting);

            seeta::ModelSetting lm_mask_setting(modelPath("face_landmarker_mask_pts5.csta"), device, dev_id);
            landmarker_mask_ = new seeta::FaceLandmarker(lm_mask_setting);

            seeta::ModelSetting rec_mask_setting(modelPath("face_recognizer_mask.csta"), device, dev_id);
            recognizer_mask_ = new seeta::FaceRecognizer(rec_mask_setting);
            database_mask_ = new seeta::FaceDatabase(rec_mask_setting);

            CVEDIX_INFO(cvedix_utils::string_format("[%s] Recognition pipeline loaded (std=%d, mask=%d features)",
                        node_name.c_str(), recognizer_std_->GetExtractFeatureSize(), recognizer_mask_->GetExtractFeatureSize()));

            engines_initialized_ = true;

            // ──────── Auto-load database ────────
            if (!db_path_.empty()) {
                std::string std_path = db_path_ + ".std";
                std::string mask_path = db_path_ + ".mask";
                if (std::filesystem::exists(std_path) || std::filesystem::exists(mask_path)) {
                    if (std::filesystem::exists(std_path) && database_std_) {
                        database_std_->Load(std_path.c_str());
                        loadNameMap(std_path + ".names", id_to_name_std_);
                    }
                    if (std::filesystem::exists(mask_path) && database_mask_) {
                        database_mask_->Load(mask_path.c_str());
                        loadNameMap(mask_path + ".names", id_to_name_mask_);
                    }
                    db_enabled_ = (database_std_ && database_std_->Count() > 0) ||
                                  (database_mask_ && database_mask_->Count() > 0);
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Database loaded (std=%zu, mask=%zu)",
                                node_name.c_str(),
                                database_std_ ? database_std_->Count() : 0,
                                database_mask_ ? database_mask_->Count() : 0));
                }
            }

            return true;

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to init SeetaFace6: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    // ──────────────────────────────────────────────────────────────
    //  Main pipeline
    // ──────────────────────────────────────────────────────────────
    void cvedix_face_analysis_node::run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch)
    {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        if (!engines_initialized_ && !initEngines()) return;

        // ── Frame skip: reuse cached results for non-analysis frames ──
        frame_counter_++;
        if (frame_skip_ > 1 && (frame_counter_ % frame_skip_) != 1 && !cached_targets_.empty()) {
            for (auto& frame_meta : frame_meta_with_batch) {
                for (auto& ct : cached_targets_) {
                    frame_meta->face_targets.push_back(ct->clone());
                }
            }
            return;
        }

        // ── Clear cache for fresh analysis ──
        cached_targets_.clear();

        for (auto& frame_meta : frame_meta_with_batch) {
            if (frame_meta->frame.empty()) continue;

            cv::Mat frame = frame_meta->frame;
            SeetaImageData simg = cvMatToSeeta(frame);

            // Step 1: Detect all faces
            SeetaFaceInfoArray faces = detector_->detect(simg);

            for (int i = 0; i < faces.size; i++) {
                const auto& face = faces.data[i];
                const SeetaRect& rect = face.pos;

                int x = std::max(rect.x, 0);
                int y = std::max(rect.y, 0);
                int w = std::min(rect.width, frame.cols - x);
                int h = std::min(rect.height, frame.rows - y);
                if (w <= 0 || h <= 0) continue;

                // Step 2: Check mask
                float ms = 0.0f;
                bool has_mask = mask_detector_->detect(simg, rect, &ms);

                // Step 3: Landmarks (5-point)
                std::vector<SeetaPointF> points = landmarker_->mark(simg, rect);
                std::vector<std::pair<int, int>> keypoints;
                keypoints.reserve(points.size());
                for (const auto& pt : points)
                    keypoints.emplace_back(static_cast<int>(pt.x), static_cast<int>(pt.y));

                // Step 4: Age prediction
                int age = -1;
                if (enable_age_gender_ && age_predictor_) {
                    age_predictor_->PredictAgeWithCrop(simg, points.data(), age);
                }

                // Step 5: Gender prediction
                int gender_val = -1;
                std::string gender_str = "";
                if (enable_age_gender_ && gender_predictor_) {
                    seeta::GenderPredictor::GENDER g;
                    if (gender_predictor_->PredictGenderWithCrop(simg, points.data(), g)) {
                        gender_val = static_cast<int>(g);
                        gender_str = (g == seeta::GenderPredictor::MALE) ? "Male" : "Female";
                    }
                }

                // Step 6: Eye state
                int left_eye = -1, right_eye = -1;
                if (enable_eye_state_ && eye_detector_) {
                    seeta::EyeStateDetector::EYE_STATE ls, rs;
                    eye_detector_->Detect(simg, points.data(), ls, rs);
                    left_eye = static_cast<int>(ls);
                    right_eye = static_cast<int>(rs);
                }

                // Step 7: Head pose
                float yaw = 0, pitch = 0, roll = 0;
                bool pose_valid = false;
                if (enable_pose_ && pose_estimator_) {
                    pose_estimator_->Estimate(simg, rect, &yaw, &pitch, &roll);
                    pose_valid = true;
                }

                // Step 8: Anti-spoofing
                int liveness = -1;
                float clarity = 0, reality = 0;
                if (enable_anti_spoofing_ && anti_spoofing_) {
                    auto status = anti_spoofing_->Predict(simg, rect, points.data());
                    liveness = static_cast<int>(status);
                    anti_spoofing_->GetPreFrameScore(&clarity, &reality);
                }

                // Step 9: Recognition
                seeta::FaceLandmarker* rec_lm = has_mask ? landmarker_mask_ : landmarker_;
                seeta::FaceRecognizer* recognizer = has_mask ? recognizer_mask_ : recognizer_std_;
                seeta::FaceDatabase* database = has_mask ? database_mask_ : database_std_;
                auto& id_to_name = has_mask ? id_to_name_mask_ : id_to_name_std_;

                std::vector<SeetaPointF> rec_points = rec_lm->mark(simg, rect);
                SeetaPointF* pts_for_rec = rec_points.data();

                int feature_size = recognizer->GetExtractFeatureSize();
                std::vector<float> features(feature_size);
                bool extracted = recognizer->Extract(simg, pts_for_rec, features.data());

                std::string identity = "";
                float identify_score = 0.0f;
                if (db_enabled_ && database && database->Count() > 0 && extracted) {
                    float similarity = 0.0f;
                    int64_t idx = database->Query(simg, pts_for_rec, &similarity);
                    if (idx >= 0 && similarity >= similarity_threshold_) {
                        auto it = id_to_name.find(idx);
                        identity = (it != id_to_name.end()) ? it->second : "ID_" + std::to_string(idx);
                        identify_score = similarity;
                    }
                }

                // Create face target with all attributes
                auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                    x, y, w, h, face.score, keypoints,
                    extracted ? features : std::vector<float>()
                );

                face_target->identify = identity;
                face_target->identify_score = identify_score;
                face_target->age = age;
                face_target->gender = gender_val;
                face_target->gender_str = gender_str;
                face_target->left_eye_state = left_eye;
                face_target->right_eye_state = right_eye;
                face_target->yaw = yaw;
                face_target->pitch = pitch;
                face_target->roll = roll;
                face_target->pose_valid = pose_valid;
                face_target->liveness_status = liveness;
                face_target->liveness_clarity = clarity;
                face_target->liveness_reality = reality;
                face_target->wearing_mask = has_mask;
                face_target->mask_score = ms;

                frame_meta->face_targets.push_back(face_target);

                // Cache for frame skipping
                cached_targets_.push_back(face_target);
            }
        }
    }

    // ──────────────────────────────────────────────────────────────
    //  Face Database API
    // ──────────────────────────────────────────────────────────────

    int64_t cvedix_face_analysis_node::registerFace(const cv::Mat& image, const std::string& name) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engines_initialized_ && !initEngines()) return -1;

        SeetaImageData simg = cvMatToSeeta(image);
        SeetaFaceInfoArray faces = detector_->detect(simg);
        if (faces.size == 0) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] No face in registration image for '%s'", node_name.c_str(), name.c_str()));
            return -1;
        }

        int best_idx = 0, best_area = 0;
        for (int i = 0; i < faces.size; i++) {
            int area = faces.data[i].pos.width * faces.data[i].pos.height;
            if (area > best_area) { best_area = area; best_idx = i; }
        }
        const SeetaRect& best_face = faces.data[best_idx].pos;

        std::vector<SeetaPointF> pts_std = landmarker_->mark(simg, best_face);
        int64_t idx_std = database_std_->Register(simg, pts_std.data());
        if (idx_std >= 0) id_to_name_std_[idx_std] = name;

        std::vector<SeetaPointF> pts_mask = landmarker_mask_->mark(simg, best_face);
        int64_t idx_mask = database_mask_->Register(simg, pts_mask.data());
        if (idx_mask >= 0) id_to_name_mask_[idx_mask] = name;

        if (idx_std >= 0 || idx_mask >= 0) {
            db_enabled_ = true;
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Registered '%s' (std=%ld, mask=%ld)",
                        node_name.c_str(), name.c_str(), idx_std, idx_mask));
        }
        return idx_std;
    }

    bool cvedix_face_analysis_node::deleteFace(int64_t face_id_std, int64_t face_id_mask) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        bool deleted = false;
        if (database_std_ && face_id_std >= 0) {
            if (database_std_->Delete(face_id_std) > 0) {
                if (face_id_mask < 0) {
                    auto it = id_to_name_std_.find(face_id_std);
                    if (it != id_to_name_std_.end()) {
                        for (auto& [mid, mname] : id_to_name_mask_) {
                            if (mname == it->second) { face_id_mask = mid; break; }
                        }
                    }
                }
                id_to_name_std_.erase(face_id_std);
                deleted = true;
            }
        }
        if (database_mask_ && face_id_mask >= 0) {
            if (database_mask_->Delete(face_id_mask) > 0) {
                id_to_name_mask_.erase(face_id_mask);
                deleted = true;
            }
        }
        return deleted;
    }

    void cvedix_face_analysis_node::clearDatabase() {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (database_std_) { database_std_->Clear(); id_to_name_std_.clear(); }
        if (database_mask_) { database_mask_->Clear(); id_to_name_mask_.clear(); }
    }

    size_t cvedix_face_analysis_node::getDatabaseSize() const {
        return database_std_ ? database_std_->Count() : 0;
    }

    bool cvedix_face_analysis_node::saveDatabase(const std::string& path) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        std::string base = path.empty() ? db_path_ : path;
        if (base.empty()) return false;
        bool ok = true;
        if (database_std_) {
            ok &= database_std_->Save((base + ".std").c_str());
            saveNameMap(base + ".std.names", id_to_name_std_);
        }
        if (database_mask_) {
            ok &= database_mask_->Save((base + ".mask").c_str());
            saveNameMap(base + ".mask.names", id_to_name_mask_);
        }
        return ok;
    }

    bool cvedix_face_analysis_node::loadDatabase(const std::string& path) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        if (!engines_initialized_ && !initEngines()) return false;
        db_path_ = path;
        bool ok = true;
        if (database_std_ && std::filesystem::exists(path + ".std")) {
            ok &= database_std_->Load((path + ".std").c_str());
            loadNameMap(path + ".std.names", id_to_name_std_);
        }
        if (database_mask_ && std::filesystem::exists(path + ".mask")) {
            ok &= database_mask_->Load((path + ".mask").c_str());
            loadNameMap(path + ".mask.names", id_to_name_mask_);
        }
        db_enabled_ = (database_std_ && database_std_->Count() > 0) ||
                       (database_mask_ && database_mask_->Count() > 0);
        return ok;
    }

    void cvedix_face_analysis_node::setDatabaseEnabled(bool enabled) { db_enabled_ = enabled; }
    void cvedix_face_analysis_node::setSimilarityThreshold(float t) { similarity_threshold_ = t; }
    void cvedix_face_analysis_node::setMinFaceSize(int size) {
        std::lock_guard<std::mutex> lock(engine_mutex_);
        min_face_size_ = size;
        if (detector_) detector_->set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, size);
    }

}

#endif // CVEDIX_WITH_FACE
