/**
 * @file cvedix_milvus_vector_search_node.h
 * @brief Generic large-scale vector search node powered by Milvus vector database
 * 
 * This node performs entity identification by searching embeddings against
 * a Milvus vector database. It is designed as a generic, reusable component
 * that can work with ANY type of embedding — faces, vehicles, persons, etc.
 * 
 * Architecture:
 *   - Sits AFTER any node that extracts embeddings (face_analysis, vehicle_reid, etc.)
 *   - Reads embeddings from configurable target source (face_targets or targets)
 *   - Batch-searches all embeddings in Milvus (1 gRPC call per frame)
 *   - Updates target.identify and target.identify_score
 *   - Passes enriched frame_meta to downstream nodes (OSD, broker, etc.)
 * 
 * Key features:
 *   - **Configurable schema**: users can customize all Milvus column names
 *   - **Multiple target sources**: works with face_targets, object targets, or custom
 *   - **Batch search**: all targets per frame in a single gRPC call
 *   - **Auto-reconnect**: graceful degradation if Milvus is unreachable
 *   - **CRUD API**: register, delete, query entities at runtime
 *   - **Auto-collection**: creates collection + HNSW index if not exists
 * 
 * Pipeline examples:
 * @code
 *   // === Face Search Pipeline ===
 *   auto face_search = std::make_shared<cvedix_milvus_vector_search_node>(
 *       "face_search",
 *       "localhost:19530", "cvedix_faces", 1024, 0.70f, 1, "IP", "",
 *       VectorTargetSource::FACE_TARGETS
 *   );
 *   
 *   // === Vehicle ReID Pipeline ===
 *   milvus_schema_config vehicle_schema;
 *   vehicle_schema.label_field = "plate_number";
 *   
 *   auto vehicle_search = std::make_shared<cvedix_milvus_vector_search_node>(
 *       "vehicle_search",
 *       "localhost:19530", "cvedix_vehicles", 256, 0.80f, 1, "IP", "",
 *       VectorTargetSource::OBJECT_TARGETS,
 *       vehicle_schema
 *   );
 * @endcode
 * 
 * @section milvus_requirements Requirements
 * - Milvus server v2.5+ running (Docker recommended)
 * - Build with -DCVEDIX_WITH_MILVUS=ON
 * - milvus-sdk-cpp built in third_party/milvus_sdk/build/
 * 
 * @section milvus_performance Performance
 * | DB Size  | Search Latency (5 queries) | QPS   |
 * |----------|----------------------------|-------|
 * | 10K      | ~5ms                       | 5000  |
 * | 1M       | ~10ms                      | 1500  |
 * | 100M     | ~15ms                      | 300   |
 * 
 * @see cvedix_face_recognizer_node For small-scale local face recognition
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
#include <functional>

#include <opencv2/core.hpp>

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include "cvedix/objects/cvedix_frame_target.h"

// Forward declarations for Milvus SDK (avoid header dependency in .h)
namespace milvus {
    class MilvusClientV2;
    class Status;
}

namespace cvedix_nodes {

    // ═══════════════════════════════════════════════════════════════
    //  Configurable Schema
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Milvus collection schema configuration
     * 
     * Allows users to customize column names in the Milvus collection.
     * Default values match the original face search node for backward compatibility.
     */
    struct milvus_schema_config {
        std::string primary_field   = "id";          ///< Primary key field name
        std::string vector_field    = "embedding";   ///< Vector embedding field name
        std::string label_field     = "name";        ///< Label/identity field (e.g., "name", "plate_number")
        std::string metadata_field  = "metadata";    ///< JSON metadata field name
        std::string timestamp_field = "created_at";  ///< Timestamp field name
        
        /**
         * @brief Additional output fields to retrieve during search
         * 
         * Pair of (field_name, field_type). The field_type is informational
         * and used when auto-creating the collection.
         * 
         * Example: {{"vehicle_color", "varchar"}, {"vehicle_type", "varchar"}}
         */
        std::vector<std::pair<std::string, std::string>> extra_fields;
    };

    /**
     * @brief Specifies which target array to read embeddings from
     */
    enum class VectorTargetSource {
        FACE_TARGETS,    ///< Read from meta->face_targets (face recognition)
        OBJECT_TARGETS,  ///< Read from meta->targets (vehicle/object ReID)
    };

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
     * @brief Search result for a single entity
     */
    struct milvus_search_result {
        int64_t id = -1;                       ///< Entity ID in Milvus
        std::string label = "";                ///< Matched label (person name, plate number, etc.)
        std::string& name = label;             ///< Backward-compatible alias for label
        float score = 0.0f;                    ///< Similarity score
        std::string metadata = "";             ///< Optional JSON metadata
        
        /**
         * @brief Extra field values retrieved from search
         * Key = field name, Value = string representation
         */
        std::unordered_map<std::string, std::string> extra_fields;
    };

    // Backward-compatible alias
    using milvus_face_result = milvus_search_result;

    /**
     * @brief Generic large-scale vector search node using Milvus vector database
     * 
     * Processes incoming embeddings by searching against a Milvus collection.
     * Supports millions of registered entities with sub-10ms search latency.
     * 
     * Works with ANY embedding type:
     * - Face embeddings (SeetaFace6, ArcFace, SFace)
     * - Vehicle ReID embeddings
     * - Person ReID embeddings
     * - Custom feature vectors
     * 
     * @note Thread-safe: all Milvus operations protected by mutex
     * @see cvedix_node Base class
     */
    class cvedix_milvus_vector_search_node : public cvedix_node
    {
    private:
        // ── Milvus Client ──
        std::shared_ptr<milvus::MilvusClientV2> client_;

        // ── Auto-Register Mode ──
        bool auto_register_enabled_ = false;   ///< Auto-register unknown entities with UUID
        std::mt19937 rng_{std::random_device{}()};
        std::string generateUUID();

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
         * @param embeddings Vector of embeddings to search
         * @return Vector of results (one per input embedding)
         */
        std::vector<milvus_search_result> batchSearch(
            const std::vector<std::vector<float>>& embeddings);

    protected:
        // ── Configuration (accessible to subclasses) ──
        milvus_config config_;
        milvus_schema_config schema_;
        VectorTargetSource target_source_;
        float similarity_threshold_;           ///< Minimum score for positive match
        int top_k_;                            ///< Number of nearest neighbors to retrieve

        /**
         * @brief Process frame: search all embeddings in Milvus
         * 
         * Reads embeddings from the configured target source, batch-searches
         * in Milvus, and writes results back to the targets.
         * 
         * If Milvus is unreachable, passes frame through unmodified.
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
            std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        /**
         * @brief Construct generic Milvus vector search node
         * 
         * @param node_name Unique node name in pipeline
         * @param milvus_uri Milvus server URI (default: "localhost:19530")
         * @param collection_name Collection name (default: "cvedix_faces")
         * @param dimension Embedding dimension (default: 1024)
         * @param similarity_threshold Minimum match score (default: 0.70)
         * @param top_k Number of candidates to retrieve (default: 1)
         * @param metric_type Distance metric: "IP" or "L2" (default: "IP")
         * @param token Authentication token (default: empty)
         * @param target_source Which target array to read embeddings from (default: FACE_TARGETS)
         * @param schema Schema configuration for Milvus collection columns (default: face-compatible)
         */
        cvedix_milvus_vector_search_node(
            std::string node_name,
            std::string milvus_uri = "localhost:19530",
            std::string collection_name = "cvedix_faces",
            int dimension = 1024,
            float similarity_threshold = 0.70f,
            int top_k = 1,
            std::string metric_type = "IP",
            std::string token = "",
            VectorTargetSource target_source = VectorTargetSource::FACE_TARGETS,
            milvus_schema_config schema = milvus_schema_config{});

        virtual ~cvedix_milvus_vector_search_node();

        // ═══════════════════════════════════════════════════════════════
        //  Entity Registration API (Generic)
        // ═══════════════════════════════════════════════════════════════

        /**
         * @brief Register a single entity in Milvus
         * 
         * @param embedding Entity embedding vector (must match collection dimension)
         * @param label Entity label/identifier (person name, plate number, etc.)
         * @param metadata Optional JSON metadata (e.g., {"department":"engineering"})
         * @return Milvus entity ID (>= 0) on success, -1 on failure
         */
        int64_t registerEntity(const std::vector<float>& embedding,
                               const std::string& label,
                               const std::string& metadata = "");

        /**
         * @brief Register multiple entities in batch (much faster than individual calls)
         * 
         * @param embeddings Vector of embedding vectors
         * @param labels Vector of labels (same size as embeddings)
         * @param metadatas Optional vector of metadata strings
         * @return Vector of Milvus entity IDs (-1 for failures)
         */
        std::vector<int64_t> registerEntities(
            const std::vector<std::vector<float>>& embeddings,
            const std::vector<std::string>& labels,
            const std::vector<std::string>& metadatas = {});

        // ── Backward-compatible aliases for face search ──
        int64_t registerFace(const std::vector<float>& embedding,
                             const std::string& name,
                             const std::string& metadata = "") {
            return registerEntity(embedding, name, metadata);
        }
        std::vector<int64_t> registerFaces(
            const std::vector<std::vector<float>>& embeddings,
            const std::vector<std::string>& names,
            const std::vector<std::string>& metadatas = {}) {
            return registerEntities(embeddings, names, metadatas);
        }

        // ═══════════════════════════════════════════════════════════════
        //  Entity Deletion API
        // ═══════════════════════════════════════════════════════════════

        /**
         * @brief Delete an entity by its Milvus entity ID
         * @param entity_id Entity ID returned by registerEntity()
         * @return true if successfully deleted
         */
        bool deleteEntity(int64_t entity_id);

        /**
         * @brief Delete all entities with a given label
         * @param label Label to match for deletion
         * @return Number of entities deleted
         */
        int deleteEntityByLabel(const std::string& label);

        // ── Backward-compatible aliases ──
        bool deleteFace(int64_t face_id) { return deleteEntity(face_id); }
        int deleteFaceByName(const std::string& name) { return deleteEntityByLabel(name); }

        // ═══════════════════════════════════════════════════════════════
        //  Query API
        // ═══════════════════════════════════════════════════════════════

        /**
         * @brief Search for a single embedding
         * @param embedding Query embedding vector
         * @param top_k Number of results (default: use node setting)
         * @return Vector of matching results sorted by score
         */
        std::vector<milvus_search_result> searchEntity(
            const std::vector<float>& embedding, int top_k = -1);

        // ── Backward-compatible alias ──
        std::vector<milvus_search_result> searchFace(
            const std::vector<float>& embedding, int top_k = -1) {
            return searchEntity(embedding, top_k);
        }

        /**
         * @brief Get total number of registered entities
         * @return Entity count, or -1 if query fails
         */
        int64_t getDatabaseSize();

        // ═══════════════════════════════════════════════════════════════
        //  Collection Management
        // ═══════════════════════════════════════════════════════════════

        /**
         * @brief Manually create collection and index
         * @return true if successful (or already exists)
         */
        bool createCollection();

        /**
         * @brief Drop the entire collection (DESTRUCTIVE!)
         * @return true if successfully dropped
         */
        bool dropCollection();

        // ═══════════════════════════════════════════════════════════════
        //  Configuration
        // ═══════════════════════════════════════════════════════════════

        void setSimilarityThreshold(float threshold) { similarity_threshold_ = threshold; }
        float getSimilarityThreshold() const { return similarity_threshold_; }

        void setTopK(int k) { top_k_ = k; }
        int getTopK() const { return top_k_; }

        void setReconnectInterval(int seconds) { reconnect_interval_sec_ = seconds; }

        /**
         * @brief Set target source at runtime
         * @param source Which target array to read embeddings from
         */
        void setTargetSource(VectorTargetSource source) { target_source_ = source; }
        VectorTargetSource getTargetSource() const { return target_source_; }

        /**
         * @brief Get/set schema configuration
         */
        void setSchemaConfig(const milvus_schema_config& schema) { schema_ = schema; }
        const milvus_schema_config& getSchemaConfig() const { return schema_; }

        /**
         * @brief Enable auto-register mode
         * 
         * When enabled, entities that are NOT found in Milvus (below threshold)
         * will be automatically registered with a generated UUID label.
         * Useful for testing and surveillance scenarios.
         */
        void setAutoRegister(bool enabled) { auto_register_enabled_ = enabled; }
        bool isAutoRegisterEnabled() const { return auto_register_enabled_; }

        /**
         * @brief Get connection status
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
