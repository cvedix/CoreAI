/**
 * @file cvedix_face_recognizer_node.h
 * @brief All-in-one face recognition node using SeetaFace6 with dual-model support
 * 
 * Full face pipeline: detection → mask check → landmarks → feature extraction → database matching.
 * Supports both standard (no mask) and masked face recognition via dual models.
 * Uses SeetaFace6 (BSD-2-Clause) as the inference backend instead of OpenCV DNN.
 * 
 * @section face_recognizer_usage Usage Example
 * @code
 * auto recognizer = std::make_shared<cvedix_face_recognizer_node>(
 *     "face_recognizer",
 *     "/path/to/seetaface6/models",   // model directory
 *     "/path/to/facedb",              // database path (auto-loads .std/.mask if exist)
 *     0.70f,                           // similarity threshold
 *     40                               // min face size
 * );
 * recognizer->attach_to({source_node});
 * 
 * // Register a face (auto-extracts both standard + mask features)
 * cv::Mat photo = cv::imread("person.jpg");
 * recognizer->registerFace(photo, "John");
 * 
 * // At runtime: auto-detects mask → routes to appropriate model
 * @endcode
 * 
 * @note Requires pre-built SeetaFace6 libraries. Build with -DCVEDIX_WITH_FACE=ON.
 * @note Model files (.csta) must be downloaded separately (~250MB).
 * 
 * @see cvedix_face_detector_node OpenCV-based face detection (lighter, no recognition)
 */

#pragma once

#ifdef CVEDIX_WITH_FACE

#include <mutex>
#include <unordered_map>
#include <opencv2/core.hpp>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"

// SeetaFace6 headers (included directly because SeetaFace6 uses versioned namespaces)
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/FaceDatabase.h>
#include <seeta/MaskDetector.h>

#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <set>
#include <map>

namespace cvedix_nodes {

    class cvedix_milvus_vector_search_node;

    enum class FaceRecognizerMode {
        SYNC,
        ASYNC
    };

    /**
     * @brief All-in-one face recognition node with dual-model mask support
     * 
     * Pipeline per face:
     * 1. Face detection (FaceDetector)
     * 2. Mask detection (MaskDetector) → routes to appropriate model set
     * 3. Landmark localization (standard or mask FaceLandmarker)
     * 4. Feature extraction (standard or mask FaceRecognizer)
     * 5. Database matching (standard or mask FaceDatabase)
     * 
     * Registration: extracts features with BOTH models → stores in BOTH databases.
     * Recognition: auto-selects the correct model based on mask detection result.
     * 
     * @note SeetaFace6 objects are NOT thread-safe; a mutex protects all operations.
     */
    class cvedix_face_recognizer_node : public cvedix_primary_infer_node
    {
    private:
        // ── SeetaFace6 engines (lazy-initialized) ──
        seeta::FaceDetector*    detector_        = nullptr;
        seeta::MaskDetector*    mask_detector_   = nullptr;

        // Standard engines (for unmasked faces)
        seeta::FaceLandmarker*  landmarker_std_  = nullptr;
        seeta::FaceRecognizer*  recognizer_std_  = nullptr;
        seeta::FaceDatabase*    database_std_    = nullptr;

        // Mask engines (for masked faces)
        seeta::FaceLandmarker*  landmarker_mask_ = nullptr;
        seeta::FaceRecognizer*  recognizer_mask_ = nullptr;
        seeta::FaceDatabase*    database_mask_   = nullptr;

        // ── Configuration ──
        std::string model_dir_;               ///< Directory containing .csta model files
        float       similarity_threshold_;    ///< Threshold for face matching (default: 0.70)
        int         min_face_size_;            ///< Minimum face pixel size (default: 40)
        bool        use_68_landmarks_;         ///< Use 68-point landmarks instead of 5 (default: false)
        bool        use_gpu_;                  ///< Use GPU for inference (requires CUDA build)
        int         gpu_id_;                   ///< GPU device ID (default: 0)

        // ── Face database ──
        bool        db_enabled_ = false;      ///< Whether face database matching is active
        std::string db_path_;                 ///< Base path for loading/saving face databases
        std::unordered_map<int64_t, std::string> id_to_name_std_;   ///< Standard DB: index → name
        std::unordered_map<int64_t, std::string> id_to_name_mask_;  ///< Mask DB: index → name

        // ── Thread safety ──
        std::mutex  engine_mutex_;

        // ── Milvus Integration ──
        std::shared_ptr<cvedix_milvus_vector_search_node> milvus_node_;

        // ── Async Processing ──
        FaceRecognizerMode mode_;
        struct FaceIdentityResult {
            std::string label = "[SEARCHING...]";
            float score = 0.0f;
            bool resolved = false;
        };
        struct AsyncSearchTask {
            int target_id;
            bool wearing_mask;
            cv::Mat crop;
            std::vector<SeetaPointF> points;
        };
        
        std::map<int, FaceIdentityResult> identity_cache_;
        std::mutex cache_mtx_;
        std::queue<AsyncSearchTask> search_queue_;
        std::set<int> pending_;
        std::mutex queue_mtx_;
        std::condition_variable queue_cv_;
        std::thread worker_;
        std::atomic<bool> worker_running_{false};
        std::deque<int> recent_faces_;
        int max_history_ = 100;
        int frame_count_ = 0;

        void workerLoop();

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
        cvedix_face_recognizer_node(std::string node_name,
                                     std::string model_dir,
                                     std::string db_path = "",
                                     float similarity_threshold = 0.70f,
                                     int min_face_size = 40,
                                     bool use_68_landmarks = false,
                                     bool use_gpu = false,
                                     int gpu_id = 0,
                                     FaceRecognizerMode mode = FaceRecognizerMode::SYNC,
                                     std::shared_ptr<cvedix_milvus_vector_search_node> milvus_node = nullptr);

        ~cvedix_face_recognizer_node();

        /**
         * @brief Check if the node runs in asynchronous mode
         */
        virtual bool is_async() const override { return mode_ == FaceRecognizerMode::ASYNC; }

        /**
         * @brief Register a face (auto-extracts features with BOTH standard and mask models)
         * @param image BGR image containing a face (should be WITHOUT mask for best results)
         * @param name Person's name/identifier
         * @return Standard DB index (>= 0) on success, -1 on failure
         */
        int64_t registerFace(const cv::Mat& image, const std::string& name);

        /**
         * @brief Delete a registered face from BOTH databases
         * @param face_id_std Standard DB index returned by registerFace()
         * @param face_id_mask Mask DB index (if -1, will try to find matching name)
         * @return true if successfully deleted from at least one database
         */
        bool deleteFace(int64_t face_id_std, int64_t face_id_mask = -1);

        void clearDatabase();
        size_t getDatabaseSize() const;

        /**
         * @brief Save both databases. Creates: path.std, path.mask, path.std.names, path.mask.names
         */
        bool saveDatabase(const std::string& path = "");

        /**
         * @brief Load both databases from base path
         */
        bool loadDatabase(const std::string& path);

        void setDatabaseEnabled(bool enabled);
        void setSimilarityThreshold(float threshold);
        void setMinFaceSize(int size);
    };

}

#endif // CVEDIX_WITH_FACE
