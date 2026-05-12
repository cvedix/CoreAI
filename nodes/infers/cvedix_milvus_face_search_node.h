/**
 * @file cvedix_milvus_face_search_node.h
 * @brief Large-scale face search node powered by Milvus vector database
 * 
 * This node performs face identification by searching face embeddings against
 * a Milvus vector database. It is designed for large-scale deployments where
 * the local SeetaFace6 FaceDatabase is insufficient (>10K faces).
 * 
 * Architecture:
 *   - Sits AFTER any node that extracts face embeddings (face_analysis, face_recognizer)
 *   - Receives frame_meta with face_targets containing embeddings
 *   - Batch-searches all embeddings in Milvus (1 gRPC call per frame)
 *   - Updates face_target.identify and face_target.identify_score
 *   - Passes enriched frame_meta to downstream nodes (OSD, broker, etc.)
 * 
 * Pipeline example:
 * @code
 *   // Large-scale face search pipeline
 *   auto src = std::make_shared<cvedix_file_src_node>("src", 0, "video.mp4");
 *   auto analyzer = std::make_shared<cvedix_face_analysis_node>(
 *       "face_analyzer", "/models/seetaface6/sf3.0_models");
 *   auto milvus_search = std::make_shared<cvedix_milvus_face_search_node>(
 *       "milvus_search",
 *       "localhost:19530",     // Milvus server
 *       "cvedix_faces",        // collection name
 *       1024,                  // embedding dimension (SeetaFace6 default)
 *       0.70f                  // similarity threshold
 *   );
 *   auto osd = std::make_shared<cvedix_osd_node>("osd");
 *   auto des = std::make_shared<cvedix_screen_des_node>("des", 0);
 *   
 *   analyzer->attach_to({src});
 *   milvus_search->attach_to({analyzer});
 *   osd->attach_to({milvus_search});
 *   des->attach_to({osd});
 *   
 *   // Register faces
 *   milvus_search->registerFace(embedding_vector, "John Doe");
 * @endcode
 * 
 * @section milvus_requirements Requirements
 * - Milvus server v2.5+ running (Docker recommended)
 * - Build with -DCVEDIX_WITH_MILVUS=ON
 * - milvus-sdk-cpp built in third_party/milvus_sdk/build/
 * 
 * @section milvus_performance Performance
 * | DB Size  | Search Latency (5 faces) | QPS   |
 * |----------|--------------------------|-------|
 * | 10K      | ~5ms                     | 5000  |
 * | 1M       | ~10ms                    | 1500  |
 * | 100M     | ~15ms                    | 300   |
 * 
 * @note This node does NOT replace cvedix_face_recognizer_node.
 *       Use this when you need to search >10K faces or share DB across pipelines.
 * 
 * @see cvedix_face_recognizer_node For small-scale local face recognition
 * @see cvedix_face_analysis_node For face attribute extraction
 * @see https://milvus.io Milvus vector database
 */

#pragma once

#ifdef CVEDIX_WITH_MILVUS

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <unordered_map>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"

// Forward declarations for Milvus SDK (avoid header dependency in .h)
namespace milvus {
    class MilvusClientV2;
    class Status;
}

namespace cvedix_nodes {

    /**
     * @brief Milvus connection configuration
     */
    struct milvus_config {
        std::string uri = "localhost:19530";   ///< Milvus server URI (host:port)
        std::string token = "";                ///< Authentication token (user:password)
        std::string collection_name = "cvedix_faces"; ///< Collection name
        int dimension = 1024;                  ///< Embedding vector dimension
        std::string metric_type = "IP";        ///< Distance metric: "IP" (Inner Product) or "L2"
        std::string index_type = "HNSW";       ///< Index type: "HNSW", "IVF_FLAT", "FLAT"
        int hnsw_m = 16;                       ///< HNSW M parameter (neighbors per node)
        int hnsw_ef_construction = 256;        ///< HNSW efConstruction (build-time quality)
        int hnsw_ef_search = 128;              ///< HNSW ef parameter (search-time quality)
        int connect_timeout_ms = 5000;         ///< Connection timeout
        int search_timeout_ms = 100;           ///< Per-search timeout
    };

    /**
     * @brief Search result for a single face
     */
    struct milvus_face_result {
        int64_t id = -1;                       ///< Face ID in Milvus
        std::string name = "";                 ///< Registered person name
        float score = 0.0f;                    ///< Similarity score
        std::string metadata = "";             ///< Optional JSON metadata
    };

    /**
     * @brief Large-scale face search node using Milvus vector database
     * 
     * Processes incoming face embeddings by searching against a Milvus collection.
     * Supports millions of registered faces with sub-10ms search latency.
     * 
     * Key features:
     * - Batch search: all faces per frame in a single gRPC call
     * - Auto-reconnect: graceful degradation if Milvus is unreachable
     * - CRUD API: register, delete, query faces at runtime
     * - Auto-collection: creates collection + HNSW index if not exists
     * 
     * @note Thread-safe: all Milvus operations protected by mutex
     * @see cvedix_node Base class
     */
    class cvedix_milvus_face_search_node : public cvedix_node
    {
    private:
        // ── Milvus Client ──
        std::shared_ptr<milvus::MilvusClientV2> client_;
        milvus_config config_;

        // ── Search Parameters ──
        float similarity_threshold_;           ///< Minimum score for positive match
        int top_k_;                            ///< Number of nearest neighbors to retrieve

        // ── Auto-Register Mode ──
        bool auto_register_enabled_ = false;   ///< Auto-register unknown faces with UUID
        std::mt19937 rng_{std::random_device{}()};
        std::string generateUUID();

        // ── Built-in SFace Feature Extraction (optional, for faces without embeddings) ──
        cv::dnn::Net sface_net_;               ///< SFace DNN model
        bool sface_loaded_ = false;
        std::string sface_model_path_;
        int sface_feature_dim_ = 128;          ///< SFace output dimension
        bool extractEmbedding(const cv::Mat& frame, 
                              std::shared_ptr<cvedix_objects::cvedix_frame_face_target>& face);

        // ── Connection State ──
        std::atomic<bool> connected_{false};
        std::atomic<bool> collection_ready_{false};
        std::chrono::steady_clock::time_point last_connect_attempt_;
        int reconnect_interval_sec_ = 5;       ///< Seconds between reconnect attempts
        int connect_failures_ = 0;

        // ── Thread Safety ──
        mutable std::mutex client_mutex_;

        // ── Statistics ──
        std::atomic<uint64_t> total_searches_{0};
        std::atomic<uint64_t> total_matches_{0};
        std::atomic<uint64_t> total_failures_{0};
        std::atomic<double> avg_search_ms_{0.0};

        // ── Internal Helpers ──
        bool ensureConnection();
        bool ensureCollection();
        bool createCollectionInternal();
        bool createIndexInternal();

        /**
         * @brief Batch search embeddings in Milvus
         * @param embeddings Vector of face embeddings to search
         * @return Vector of results (one per input embedding)
         */
        std::vector<milvus_face_result> batchSearch(
            const std::vector<std::vector<float>>& embeddings);

    protected:
        /**
         * @brief Process frame: search all face embeddings in Milvus
         * 
         * For each face_target with non-empty embeddings:
         *   1. Collect embeddings into a batch
         *   2. Execute single Milvus search call
         *   3. Update face_target.identify and face_target.identify_score
         * 
         * If Milvus is unreachable, passes frame through unmodified.
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
            std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        /**
         * @brief Construct Milvus face search node
         * 
         * @param node_name Unique node name in pipeline
         * @param milvus_uri Milvus server URI (default: "localhost:19530")
         * @param collection_name Collection name (default: "cvedix_faces")
         * @param dimension Embedding dimension (default: 1024 for SeetaFace6)
         * @param similarity_threshold Minimum match score (default: 0.70)
         * @param top_k Number of candidates to retrieve (default: 1)
         * @param metric_type Distance metric: "IP" or "L2" (default: "IP")
         * @param token Authentication token (default: empty)
         */
        cvedix_milvus_face_search_node(
            std::string node_name,
            std::string milvus_uri = "localhost:19530",
            std::string collection_name = "cvedix_faces",
            int dimension = 1024,
            float similarity_threshold = 0.70f,
            int top_k = 1,
            std::string metric_type = "IP",
            std::string token = "");

        ~cvedix_milvus_face_search_node();

        // ── Face Registration API ──

        /**
         * @brief Register a single face in Milvus
         * 
         * @param embedding Face embedding vector (must match collection dimension)
         * @param name Person's name/identifier
         * @param metadata Optional JSON metadata (e.g., {"department":"engineering"})
         * @return Milvus entity ID (>= 0) on success, -1 on failure
         */
        int64_t registerFace(const std::vector<float>& embedding,
                             const std::string& name,
                             const std::string& metadata = "");

        /**
         * @brief Register multiple faces in batch (much faster than individual calls)
         * 
         * @param embeddings Vector of embedding vectors
         * @param names Vector of person names (same size as embeddings)
         * @param metadatas Optional vector of metadata strings
         * @return Vector of Milvus entity IDs (-1 for failures)
         */
        std::vector<int64_t> registerFaces(
            const std::vector<std::vector<float>>& embeddings,
            const std::vector<std::string>& names,
            const std::vector<std::string>& metadatas = {});

        // ── Face Deletion API ──

        /**
         * @brief Delete a face by its Milvus entity ID
         * @param face_id Entity ID returned by registerFace()
         * @return true if successfully deleted
         */
        bool deleteFace(int64_t face_id);

        /**
         * @brief Delete all faces with a given name
         * @param name Person's name to delete
         * @return Number of faces deleted
         */
        int deleteFaceByName(const std::string& name);

        // ── Query API ──

        /**
         * @brief Search for a single face embedding
         * @param embedding Query embedding vector
         * @param top_k Number of results (default: use node setting)
         * @return Vector of matching results sorted by score
         */
        std::vector<milvus_face_result> searchFace(
            const std::vector<float>& embedding, int top_k = -1);

        /**
         * @brief Get total number of registered faces
         * @return Face count, or -1 if query fails
         */
        int64_t getDatabaseSize();

        // ── Collection Management ──

        /**
         * @brief Manually create collection and index
         * @return true if successful (or already exists)
         */
        bool createCollection();

        /**
         * @brief Drop the entire face collection (DESTRUCTIVE!)
         * @return true if successfully dropped
         */
        bool dropCollection();

        // ── Configuration ──

        void setSimilarityThreshold(float threshold) { similarity_threshold_ = threshold; }
        float getSimilarityThreshold() const { return similarity_threshold_; }

        void setTopK(int k) { top_k_ = k; }
        int getTopK() const { return top_k_; }

        void setReconnectInterval(int seconds) { reconnect_interval_sec_ = seconds; }

        /**
         * @brief Enable auto-register mode
         * 
         * When enabled, faces that are NOT found in Milvus (below threshold)
         * will be automatically registered with a generated UUID name.
         * Useful for testing and surveillance scenarios.
         * 
         * @param enabled true to enable auto-registration
         */
        void setAutoRegister(bool enabled) { auto_register_enabled_ = enabled; }
        bool isAutoRegisterEnabled() const { return auto_register_enabled_; }

        /**
         * @brief Set SFace model path for built-in face feature extraction
         * 
         * When set, the node can extract face embeddings from face crops
         * that don't already have embeddings. Uses OpenCV DNN SFace model.
         * 
         * @param model_path Path to face_recognition_sface_2021dec.onnx
         */
        void setSFaceModel(const std::string& model_path);

        /**
         * @brief Get connection status
         * @return true if currently connected to Milvus
         */
        bool isConnected() const { return connected_.load(); }

        /**
         * @brief Get search statistics
         */
        uint64_t getTotalSearches() const { return total_searches_.load(); }
        uint64_t getTotalMatches() const { return total_matches_.load(); }
        double getAvgSearchMs() const { return avg_search_ms_.load(); }

        /**
         * @brief Get Milvus configuration (read-only)
         */
        const milvus_config& getConfig() const { return config_; }

        virtual std::string to_string() override;
    };

}

#endif // CVEDIX_WITH_MILVUS
