/**
 * @file cvedix_milvus_face_search_node.cpp
 * @brief Implementation of large-scale face search node using Milvus
 * 
 * Core flow:
 *   handle_frame_meta() → collect embeddings → batchSearch() → update targets
 * 
 * Connection management:
 *   - Lazy connect on first frame
 *   - Auto-reconnect with exponential backoff
 *   - Graceful degradation: pipeline continues if Milvus is down
 */

#ifdef CVEDIX_WITH_MILVUS

#include "cvedix_milvus_face_search_node.h"

// Milvus C++ SDK v2.6
#include <milvus/MilvusClientV2.h>

#include <algorithm>
#include <sstream>

namespace cvedix_nodes {

    // ═══════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════

    cvedix_milvus_face_search_node::cvedix_milvus_face_search_node(
        std::string node_name,
        std::string milvus_uri,
        std::string collection_name,
        int dimension,
        float similarity_threshold,
        int top_k,
        std::string metric_type,
        std::string token)
        : cvedix_node(node_name),
          similarity_threshold_(similarity_threshold),
          top_k_(top_k)
    {
        config_.uri = std::move(milvus_uri);
        config_.collection_name = std::move(collection_name);
        config_.dimension = dimension;
        config_.metric_type = std::move(metric_type);
        config_.token = std::move(token);

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Milvus face search node created (uri=%s, collection=%s, dim=%d, threshold=%.2f, metric=%s)",
            node_name.c_str(), config_.uri.c_str(), config_.collection_name.c_str(),
            config_.dimension, similarity_threshold_, config_.metric_type.c_str()));

        this->initialized();
    }

    cvedix_milvus_face_search_node::~cvedix_milvus_face_search_node() {
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Shutting down (total_searches=%lu, total_matches=%lu, avg_latency=%.1fms)",
            node_name.c_str(), total_searches_.load(), total_matches_.load(), avg_search_ms_.load()));

        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            if (client_) {
                client_->Disconnect();
                client_.reset();
            }
        }
        connected_ = false;
        deinitialized();
    }

    // ═══════════════════════════════════════════════════════════════
    //  Connection Management
    // ═══════════════════════════════════════════════════════════════

    bool cvedix_milvus_face_search_node::ensureConnection() {
        if (connected_.load()) return true;

        // Rate-limit reconnect attempts
        auto now = std::chrono::steady_clock::now();
        if (connect_failures_ > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_connect_attempt_).count();
            if (elapsed < reconnect_interval_sec_) {
                return false;
            }
        }

        last_connect_attempt_ = now;

        try {
            if (!client_) {
                client_ = milvus::MilvusClientV2::Create();
            }

            // Build connection parameters
            milvus::ConnectParam connect_param{config_.uri};
            if (!config_.token.empty()) {
                connect_param = milvus::ConnectParam{config_.uri, config_.token};
            }

            auto status = client_->Connect(connect_param);
            if (!status.IsOk()) {
                connect_failures_++;
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Failed to connect to Milvus at %s: %s (attempt #%d)",
                    node_name.c_str(), config_.uri.c_str(), 
                    status.Message().c_str(), connect_failures_));
                return false;
            }

            connected_ = true;
            connect_failures_ = 0;
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Connected to Milvus at %s", 
                node_name.c_str(), config_.uri.c_str()));
            return true;

        } catch (const std::exception& e) {
            connect_failures_++;
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception connecting to Milvus: %s", 
                node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_face_search_node::ensureCollection() {
        if (collection_ready_.load()) return true;
        if (!ensureConnection()) return false;

        try {
            // Check if collection exists by trying to get stats
            bool exists = false;
            milvus::HasCollectionRequest has_req;
            has_req.SetCollectionName(config_.collection_name);
            milvus::HasCollectionResponse has_res;
            auto status = client_->HasCollection(has_req, has_res);
            exists = has_res.Has();
            
            if (status.IsOk() && exists) {
                // Load collection into memory for search
                milvus::LoadCollectionRequest load_req;
                load_req.SetCollectionName(config_.collection_name);
                status = client_->LoadCollection(load_req);
                if (status.IsOk()) {
                    collection_ready_ = true;
                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] Collection '%s' loaded and ready",
                        node_name.c_str(), config_.collection_name.c_str()));
                    return true;
                }
            }

            if (!exists) {
                // Auto-create collection
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] Collection '%s' not found, creating...",
                    node_name.c_str(), config_.collection_name.c_str()));
                return createCollectionInternal();
            }

            return false;

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception checking collection: %s",
                node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_face_search_node::createCollectionInternal() {
        if (!client_ || !connected_.load()) return false;

        try {
            // Use simplified collection creation (Milvus v2.6 API)
            auto status = client_->CreateCollection(
                milvus::CreateSimpleCollectionRequest()
                    .WithCollectionName(config_.collection_name)
                    .WithPrimaryFieldName("id")
                    .WithVectorFieldName("embedding")
                    .WithDimension(config_.dimension)
            );

            if (!status.IsOk()) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Failed to create collection: %s",
                    node_name.c_str(), status.Message().c_str()));
                return false;
            }

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Collection '%s' created (dim=%d)",
                node_name.c_str(), config_.collection_name.c_str(), config_.dimension));

            // Create index
            if (!createIndexInternal()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Collection created but index creation failed",
                    node_name.c_str()));
            }

            // Load collection
            milvus::LoadCollectionRequest load_req;
                load_req.SetCollectionName(config_.collection_name);
                status = client_->LoadCollection(load_req);
            if (status.IsOk()) {
                collection_ready_ = true;
                return true;
            }

            return false;

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception creating collection: %s",
                node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_face_search_node::createIndexInternal() {
        if (!client_ || !connected_.load()) return false;

        try {
            // Create HNSW index for face embeddings
            milvus::IndexDesc index_desc;
            index_desc.SetFieldName("embedding");
            index_desc.SetIndexName("");
            index_desc.SetMetricType(milvus::MetricType::L2);
            index_desc.SetIndexType(milvus::IndexType::HNSW);

            if (config_.index_type == "HNSW") {
                index_desc.AddExtraParam("M", std::to_string(config_.hnsw_m));
                index_desc.AddExtraParam("efConstruction", 
                    std::to_string(config_.hnsw_ef_construction));
            }

            milvus::CreateIndexRequest idx_req;
            idx_req.SetCollectionName(config_.collection_name);
            idx_req.AddIndex(std::move(index_desc));
            auto status = client_->CreateIndex(idx_req);
            if (!status.IsOk()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Failed to create %s index: %s",
                    node_name.c_str(), config_.index_type.c_str(), 
                    status.Message().c_str()));
                return false;
            }

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] %s index created on 'embedding' field (M=%d, ef=%d, metric=%s)",
                node_name.c_str(), config_.index_type.c_str(),
                config_.hnsw_m, config_.hnsw_ef_construction, 
                config_.metric_type.c_str()));
            return true;

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception creating index: %s",
                node_name.c_str(), e.what()));
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Auto-Register & SFace Utils
    // ═══════════════════════════════════════════════════════════════

    std::string cvedix_milvus_face_search_node::generateUUID() {
        std::uniform_int_distribution<int> dist(0, 15);
        const char* v = "0123456789ABCDEF";
        std::string res = "Guest-";
        for (int i = 0; i < 4; i++) {
            res += v[dist(rng_)];
        }
        return res;
    }

    void cvedix_milvus_face_search_node::setSFaceModel(const std::string& model_path) {
        sface_model_path_ = model_path;
        try {
            sface_net_ = cv::dnn::readNetFromONNX(model_path);
            sface_loaded_ = !sface_net_.empty();
            if (sface_loaded_) {
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] Loaded SFace model from %s", node_name.c_str(), model_path.c_str()));
            } else {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Failed to load SFace model from %s", node_name.c_str(), model_path.c_str()));
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception loading SFace model: %s", node_name.c_str(), e.what()));
            sface_loaded_ = false;
        }
    }

    bool cvedix_milvus_face_search_node::extractEmbedding(
        const cv::Mat& frame, std::shared_ptr<cvedix_objects::cvedix_frame_face_target>& face) 
    {
        if (!sface_loaded_ || frame.empty() || !face) return false;

        // Ensure valid crop
        int x = std::max(0, face->x);
        int y = std::max(0, face->y);
        int w = std::min(frame.cols - x, face->width);
        int h = std::min(frame.rows - y, face->height);
        if (w <= 0 || h <= 0) return false;

        cv::Mat crop = frame(cv::Rect(x, y, w, h));
        if (crop.empty()) return false;

        try {
            cv::Mat blob = cv::dnn::blobFromImage(crop, 1.0, cv::Size(112, 112), cv::Scalar(0, 0, 0), true, false);
            sface_net_.setInput(blob);
            cv::Mat features = sface_net_.forward();

            // L2 Normalize
            cv::normalize(features, features, 1.0, 0.0, cv::NORM_L2);

            face->embeddings.assign((float*)features.datastart, (float*)features.dataend);
            return true;
        } catch (...) {
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Core Pipeline: handle_frame_meta
    // ═══════════════════════════════════════════════════════════════

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_milvus_face_search_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
    {
        if (!meta || meta->face_targets.empty()) {
            return meta;  // Pass through if no faces
        }

        // Collect face targets with valid embeddings
        std::vector<size_t> target_indices;
        std::vector<std::vector<float>> query_embeddings;

        for (size_t i = 0; i < meta->face_targets.size(); i++) {
            auto& face = meta->face_targets[i];
            if (face) {
                // Auto-extract features if empty and SFace is loaded
                if (face->embeddings.empty() && sface_loaded_) {
                    extractEmbedding(meta->frame, face);
                }

                if (!face->embeddings.empty()) {
                    // Skip faces that already have identification (from local DB)
                    if (!face->identify.empty() && face->identify != "Unknown") {
                        continue;
                    }
                    target_indices.push_back(i);
                    query_embeddings.push_back(face->embeddings);
                }
            }
        }

        if (query_embeddings.empty()) {
            return meta;  // No unidentified faces with embeddings
        }

        // Batch search in Milvus
        std::unique_lock<std::mutex> lock(client_mutex_);

        if (!ensureCollection()) {
            // Milvus not available — pass through without modification
            total_failures_++;
            return meta;
        }

        auto start = std::chrono::steady_clock::now();
        auto results = batchSearch(query_embeddings);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;

        // Update statistics
        total_searches_++;
        avg_search_ms_ = (avg_search_ms_.load() * (total_searches_.load() - 1) + elapsed) 
                         / total_searches_.load();

        // Apply results to face targets
        for (size_t i = 0; i < results.size() && i < target_indices.size(); i++) {
            auto& result = results[i];
            auto& face = meta->face_targets[target_indices[i]];

            if (result.id >= 0 && result.score >= similarity_threshold_) {
                face->identify = result.name;
                face->identify_score = result.score;
                total_matches_++;
            } else if (auto_register_enabled_) {
                // Auto-register unknown face
                std::string new_uuid = generateUUID();
                
                // Drop lock temporarily for registration
                lock.unlock();
                int64_t new_id = registerFace(query_embeddings[i], new_uuid, "{\"auto_registered\":true}");
                lock.lock();

                if (new_id >= 0) {
                    face->identify = new_uuid;
                    face->identify_score = 1.0f; // Newly registered, perfect match
                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] Auto-registered new face: %s (id=%ld)", node_name.c_str(), new_uuid.c_str(), new_id));
                }
            }
        }

        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_milvus_face_search_node::handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta)
    {
        return meta;  // Pass through control messages
    }

    // ═══════════════════════════════════════════════════════════════
    //  Batch Search
    // ═══════════════════════════════════════════════════════════════

    std::vector<milvus_face_result>
    cvedix_milvus_face_search_node::batchSearch(
        const std::vector<std::vector<float>>& embeddings)
    {
        std::vector<milvus_face_result> results(embeddings.size());

        if (embeddings.empty() || !client_ || !connected_.load()) {
            return results;
        }

        try {
            milvus::SearchRequest req;
            req.WithCollectionName(config_.collection_name)
               .WithAnnsField("embedding")
               .WithLimit(top_k_)
               .WithOutputFields({"name", "metadata"})
               .AddExtraParam("ef", std::to_string(config_.hnsw_ef_search));

            for (const auto& emb : embeddings) {
                req.AddFloatVector(emb);
            }

            milvus::SearchResponse response;
            auto status = client_->Search(req, response);

            if (!status.IsOk()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Milvus search failed: %s",
                    node_name.c_str(), status.Message().c_str()));

                // Mark connection as failed for auto-reconnect
                if (status.Message().find("connect") != std::string::npos ||
                    status.Message().find("unavailable") != std::string::npos) {
                    connected_ = false;
                    collection_ready_ = false;
                }
                return results;
            }

            // Parse results
            auto search_results = response.Results().Results();
            for (size_t i = 0; i < search_results.size() && i < results.size(); i++) {
                if (!search_results[i].Scores().empty()) {
                    auto& top_match = search_results[i];  // Best match
                    results[i].id = top_match.Ids().IntIDArray()[0];
                    results[i].score = top_match.Scores()[0];

                    // Extract scalar fields
                    auto name_field = std::dynamic_pointer_cast<milvus::VarCharFieldData>(top_match.OutputField("name"));
                    if (name_field && !name_field->Data().empty()) {
                        results[i].name = name_field->Data()[0];
                    }

                    auto meta_field = std::dynamic_pointer_cast<milvus::VarCharFieldData>(top_match.OutputField("metadata"));
                    if (meta_field && !meta_field->Data().empty()) {
                        results[i].metadata = meta_field->Data()[0];
                    }
                }
            }

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception during batch search: %s",
                node_name.c_str(), e.what()));
            connected_ = false;
            collection_ready_ = false;
        }

        return results;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Face Registration API
    // ═══════════════════════════════════════════════════════════════

    int64_t cvedix_milvus_face_search_node::registerFace(
        const std::vector<float>& embedding,
        const std::string& name,
        const std::string& metadata)
    {
        auto ids = registerFaces({embedding}, {name}, {metadata});
        return ids.empty() ? -1 : ids[0];
    }

    std::vector<int64_t> cvedix_milvus_face_search_node::registerFaces(
        const std::vector<std::vector<float>>& embeddings,
        const std::vector<std::string>& names,
        const std::vector<std::string>& metadatas)
    {
        std::vector<int64_t> result_ids(embeddings.size(), -1);

        if (embeddings.empty() || embeddings.size() != names.size()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] registerFaces: invalid input (embeddings=%zu, names=%zu)",
                node_name.c_str(), embeddings.size(), names.size()));
            return result_ids;
        }

        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureCollection()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Cannot register faces: Milvus not available",
                node_name.c_str()));
            return result_ids;
        }

        try {
            // Validate dimensions
            for (size_t i = 0; i < embeddings.size(); i++) {
                if (static_cast<int>(embeddings[i].size()) != config_.dimension) {
                    CVEDIX_ERROR(cvedix_utils::string_format(
                        "[%s] Embedding dimension mismatch for '%s': got %zu, expected %d",
                        node_name.c_str(), names[i].c_str(), 
                        embeddings[i].size(), config_.dimension));
                    return result_ids;
                }
            }

            // Build insert data
            // Prepare field data for Milvus insert
            std::vector<milvus::FieldDataPtr> fields;
            
            // Name field
            std::vector<std::string> name_values = names;
            fields.push_back(std::make_shared<milvus::VarCharFieldData>("name", name_values));

            // Metadata field
            std::vector<std::string> meta_values;
            meta_values.resize(embeddings.size());
            for (size_t i = 0; i < embeddings.size(); i++) {
                meta_values[i] = (i < metadatas.size()) ? metadatas[i] : "";
            }
            fields.push_back(std::make_shared<milvus::VarCharFieldData>("metadata", meta_values));

            // Timestamp field
            int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            std::vector<int64_t> timestamps(embeddings.size(), now);
            fields.push_back(std::make_shared<milvus::Int64FieldData>("created_at", timestamps));

            // Embedding vectors
            fields.push_back(std::make_shared<milvus::FloatVecFieldData>("embedding", embeddings));

            // Insert
            milvus::InsertResponse insert_response;
            milvus::InsertRequest insert_req;
            insert_req.SetCollectionName(config_.collection_name);
            insert_req.SetColumnsData(std::move(fields));
            auto status = client_->Insert(insert_req, insert_response);

            if (!status.IsOk()) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Failed to insert faces: %s",
                    node_name.c_str(), status.Message().c_str()));
                return result_ids;
            }

            // Get inserted IDs
            auto& ids = insert_response.Results().IdArray().IntIDArray();
            for (size_t i = 0; i < ids.size() && i < result_ids.size(); i++) {
                result_ids[i] = ids[i];
            }

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Registered %zu faces (first: '%s' → id=%ld)",
                node_name.c_str(), embeddings.size(), 
                names[0].c_str(), result_ids[0]));

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception registering faces: %s",
                node_name.c_str(), e.what()));
        }

        return result_ids;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Face Deletion API
    // ═══════════════════════════════════════════════════════════════

    bool cvedix_milvus_face_search_node::deleteFace(int64_t face_id) {
        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureConnection() || !client_) return false;

        try {
            std::string filter = "id == " + std::to_string(face_id);
            milvus::DeleteRequest del_req;
            del_req.SetCollectionName(config_.collection_name);
            del_req.SetFilter(filter);
            milvus::DeleteResponse del_res;
            auto status = client_->Delete(del_req, del_res);

            if (status.IsOk()) {
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] Deleted face id=%ld", node_name.c_str(), face_id));
                return true;
            } else {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Failed to delete face id=%ld: %s",
                    node_name.c_str(), face_id, status.Message().c_str()));
                return false;
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception deleting face: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    int cvedix_milvus_face_search_node::deleteFaceByName(const std::string& name) {
        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureConnection() || !client_) return 0;

        try {
            std::string filter = "name == \"" + name + "\"";
            milvus::DeleteRequest del_req;
            del_req.SetCollectionName(config_.collection_name);
            del_req.SetFilter(filter);
            milvus::DeleteResponse del_res;
            auto status = client_->Delete(del_req, del_res);

            if (status.IsOk()) {
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] Deleted faces with name='%s'",
                    node_name.c_str(), name.c_str()));
                return 1;  // Milvus doesn't return count easily
            }
            return 0;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception deleting faces by name: %s",
                node_name.c_str(), e.what()));
            return 0;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Query API
    // ═══════════════════════════════════════════════════════════════

    std::vector<milvus_face_result>
    cvedix_milvus_face_search_node::searchFace(
        const std::vector<float>& embedding, int top_k)
    {
        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureCollection()) {
            return {};
        }

        int k = (top_k > 0) ? top_k : top_k_;
        // Temporarily override top_k for this search
        int saved_top_k = top_k_;
        top_k_ = k;
        auto results = batchSearch({embedding});
        top_k_ = saved_top_k;

        return results;
    }

    int64_t cvedix_milvus_face_search_node::getDatabaseSize() {
        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureConnection() || !client_) return -1;

        try {
            milvus::GetCollectionStatsResponse stat_res;
            milvus::GetCollectionStatsRequest stat_req;
            stat_req.SetCollectionName(config_.collection_name);
            auto status = client_->GetCollectionStats(stat_req, stat_res);

            if (status.IsOk()) {
                // Return row count from stats
                return stat_res.Stats().RowCount();
            }
            return -1;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception getting DB size: %s",
                node_name.c_str(), e.what()));
            return -1;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Collection Management (Public API)
    // ═══════════════════════════════════════════════════════════════

    bool cvedix_milvus_face_search_node::createCollection() {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection()) return false;
        return createCollectionInternal();
    }

    bool cvedix_milvus_face_search_node::dropCollection() {
        std::lock_guard<std::mutex> lock(client_mutex_);

        if (!ensureConnection() || !client_) return false;

        try {
            milvus::DropCollectionRequest drop_req;
            drop_req.SetCollectionName(config_.collection_name);
            auto status = client_->DropCollection(drop_req);
            if (status.IsOk()) {
                collection_ready_ = false;
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Dropped collection '%s'",
                    node_name.c_str(), config_.collection_name.c_str()));
                return true;
            }
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to drop collection: %s",
                node_name.c_str(), status.Message().c_str()));
            return false;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception dropping collection: %s",
                node_name.c_str(), e.what()));
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Utilities
    // ═══════════════════════════════════════════════════════════════

    std::string cvedix_milvus_face_search_node::to_string() {
        std::ostringstream oss;
        oss << "cvedix_milvus_face_search_node {"
            << " name=" << node_name
            << ", uri=" << config_.uri
            << ", collection=" << config_.collection_name
            << ", dim=" << config_.dimension
            << ", threshold=" << similarity_threshold_
            << ", connected=" << (connected_.load() ? "yes" : "no")
            << ", searches=" << total_searches_.load()
            << ", matches=" << total_matches_.load()
            << ", avg_ms=" << avg_search_ms_.load()
            << " }";
        return oss.str();
    }

}

#endif // CVEDIX_WITH_MILVUS
