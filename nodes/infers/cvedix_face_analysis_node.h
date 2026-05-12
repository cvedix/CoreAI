/**
 * @file cvedix_face_analysis_node.h
 * @brief All-in-one face analysis node using SeetaFace6
 * 
 * Full face analysis pipeline: detection → mask check → landmarks → age/gender →
 * eye state → head pose → anti-spoofing → recognition (optional).
 * 
 * Uses SeetaFace6 (BSD-2-Clause) modules:
 *   - FaceDetector, FaceLandmarker, MaskDetector
 *   - AgePredictor, GenderPredictor, EyeStateDetector
 *   - PoseEstimator, FaceAntiSpoofing
 *   - FaceRecognizer, FaceDatabase (optional)
 * 
 * @section face_analysis_usage Usage Example
 * @code
 * auto analyzer = std::make_shared<cvedix_face_analysis_node>(
 *     "face_analyzer",
 *     "/path/to/seetaface6/sf3.0_models",
 *     "",       // db_path (optional)
 *     0.70f,    // similarity threshold
 *     80,       // min face size
 *     true,     // enable anti-spoofing
 *     true,     // enable age/gender
 *     true,     // enable eye state
 *     true      // enable pose estimation
 * );
 * analyzer->attach_to({source_node});
 * @endcode
 * 
 * @note Requires pre-built SeetaFace6 libraries. Build with -DCVEDIX_WITH_FACE=ON.
 * @note Model files (.csta) must be in the specified model directory.
 * 
 * @see cvedix_face_recognizer_node For recognition-only pipeline
 */

#pragma once

#ifdef CVEDIX_WITH_FACE

#include <mutex>
#include <unordered_map>
#include <opencv2/core.hpp>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"

// SeetaFace6 core headers
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/FaceDatabase.h>
#include <seeta/MaskDetector.h>

// SeetaFace6 analysis headers
#include <seeta/AgePredictor.h>
#include <seeta/GenderPredictor.h>
#include <seeta/EyeStateDetector.h>
#include <seeta/PoseEstimator.h>
#include <seeta/FaceAntiSpoofing.h>

namespace cvedix_nodes {

    /**
     * @brief All-in-one face analysis node with full SeetaFace6 pipeline
     * 
     * Pipeline per detected face:
     * 1. Face detection (FaceDetector)
     * 2. Mask detection (MaskDetector)
     * 3. Landmark localization (FaceLandmarker 5pt)
     * 4. Age prediction (AgePredictor)
     * 5. Gender prediction (GenderPredictor)
     * 6. Eye state detection (EyeStateDetector)
     * 7. Head pose estimation (PoseEstimator)
     * 8. Anti-spoofing / liveness (FaceAntiSpoofing)
     * 9. Face recognition (optional, if database loaded)
     * 
     * All attributes are stored in cvedix_frame_face_target for downstream
     * OSD rendering and analysis.
     * 
     * @note SeetaFace6 objects are NOT thread-safe; a mutex protects all operations.
     */
    class cvedix_face_analysis_node : public cvedix_primary_infer_node
    {
    private:
        // ── SeetaFace6 engines (lazy-initialized) ──

        // Core engines
        seeta::FaceDetector*       detector_         = nullptr;
        seeta::MaskDetector*       mask_detector_     = nullptr;
        seeta::FaceLandmarker*     landmarker_        = nullptr;

        // Analysis engines
        seeta::AgePredictor*       age_predictor_     = nullptr;
        seeta::GenderPredictor*    gender_predictor_  = nullptr;
        seeta::EyeStateDetector*   eye_detector_      = nullptr;
        seeta::PoseEstimator*      pose_estimator_    = nullptr;
        seeta::FaceAntiSpoofing*   anti_spoofing_     = nullptr;

        // Recognition engines (optional)
        seeta::FaceRecognizer*     recognizer_std_    = nullptr;
        seeta::FaceDatabase*       database_std_      = nullptr;
        seeta::FaceLandmarker*     landmarker_mask_   = nullptr;
        seeta::FaceRecognizer*     recognizer_mask_   = nullptr;
        seeta::FaceDatabase*       database_mask_     = nullptr;

        // ── Configuration ──
        std::string model_dir_;               ///< Directory containing .csta model files
        float       similarity_threshold_;    ///< Threshold for face matching (default: 0.70)
        int         min_face_size_;            ///< Minimum face pixel size (default: 80)
        bool        use_gpu_;                  ///< Use GPU for inference
        int         gpu_id_;                   ///< GPU device ID

        // Feature toggles
        bool        enable_anti_spoofing_;
        bool        enable_age_gender_;
        bool        enable_eye_state_;
        bool        enable_pose_;

        // ── Face database ──
        bool        db_enabled_ = false;
        std::string db_path_;
        std::unordered_map<int64_t, std::string> id_to_name_std_;
        std::unordered_map<int64_t, std::string> id_to_name_mask_;

        // ── Thread safety ──
        std::mutex  engine_mutex_;

        // ── Frame skipping for performance ──
        int         frame_skip_ = 3;       ///< Process every Nth frame (1=every frame)
        int         frame_counter_ = 0;
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_face_target>> cached_targets_;

        // ── Internal helpers ──
        bool initEngines();
        bool engines_initialized_ = false;

    protected:
        virtual void run_infer_combinations(
            const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

        // No-ops: SeetaFace6 handles its own pipeline
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override {}
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override {}
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                 const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override {}

    public:
        /**
         * @brief Construct face analysis node
         * @param node_name Unique node name
         * @param model_dir Path to SeetaFace6 model directory (containing .csta files)
         * @param db_path Base path for face database (optional, empty = no recognition)
         * @param similarity_threshold Threshold for face matching
         * @param min_face_size Minimum face pixel size for detection
         * @param enable_anti_spoofing Enable anti-spoofing / liveness detection
         * @param enable_age_gender Enable age and gender prediction
         * @param enable_eye_state Enable eye state detection
         * @param enable_pose Enable head pose estimation
         * @param use_gpu Use GPU for inference (requires CUDA build)
         * @param gpu_id GPU device ID
         */
        cvedix_face_analysis_node(std::string node_name,
                                   std::string model_dir,
                                   std::string db_path = "",
                                   float similarity_threshold = 0.70f,
                                   int min_face_size = 80,
                                   bool enable_anti_spoofing = true,
                                   bool enable_age_gender = true,
                                   bool enable_eye_state = true,
                                   bool enable_pose = true,
                                   bool use_gpu = false,
                                   int gpu_id = 0);

        ~cvedix_face_analysis_node();

        // ── Face Database API ──

        /**
         * @brief Register a face (extracts features with BOTH standard and mask models)
         * @param image BGR image containing a face
         * @param name Person's name/identifier
         * @return Standard DB index (>= 0) on success, -1 on failure
         */
        int64_t registerFace(const cv::Mat& image, const std::string& name);

        /**
         * @brief Delete a registered face from BOTH databases
         */
        bool deleteFace(int64_t face_id_std, int64_t face_id_mask = -1);

        void clearDatabase();
        size_t getDatabaseSize() const;

        /**
         * @brief Save both databases
         */
        bool saveDatabase(const std::string& path = "");

        /**
         * @brief Load both databases from base path
         */
        bool loadDatabase(const std::string& path);

        void setDatabaseEnabled(bool enabled);
        void setSimilarityThreshold(float threshold);
        void setMinFaceSize(int size);

        // ── Feature toggle API ──
        void setAntiSpoofingEnabled(bool enabled) { enable_anti_spoofing_ = enabled; }
        void setAgeGenderEnabled(bool enabled) { enable_age_gender_ = enabled; }
        void setEyeStateEnabled(bool enabled) { enable_eye_state_ = enabled; }
        void setPoseEnabled(bool enabled) { enable_pose_ = enabled; }

        /**
         * @brief Set frame skip interval for performance
         * @param n Process every Nth frame (1=every frame, 3=skip 2, etc.)
         *          Skipped frames reuse the last analysis results.
         *          Higher values = better FPS but less responsive.
         */
        void setFrameSkip(int n) { frame_skip_ = std::max(1, n); }
    };

}

#endif // CVEDIX_WITH_FACE
