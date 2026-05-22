/**
 * @file cvedix_milvus_vector_search_node.cpp
 * @brief Generic vector search node using Milvus - supports face, vehicle, any embeddings
 */

#ifdef CVEDIX_WITH_MILVUS

#include "cvedix_milvus_vector_search_node.h"
#include <milvus/MilvusClientV2.h>
#include <algorithm>
#include <sstream>
#include <set>

namespace cvedix_nodes {

    // ═══════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════

    cvedix_milvus_vector_search_node::cvedix_milvus_vector_search_node(
        std::string node_name, std::string milvus_uri, std::string collection_name,
        int dimension, float similarity_threshold, int top_k,
        std::string metric_type, std::string token,
        VectorTargetSource target_source, milvus_schema_config schema)
        : cvedix_node(node_name),
          similarity_threshold_(similarity_threshold),
          top_k_(top_k),
          target_source_(target_source),
          schema_(std::move(schema))
    {
        config_.uri = std::move(milvus_uri);
        config_.collection_name = std::move(collection_name);
        config_.dimension = dimension;
        config_.metric_type = std::move(metric_type);
        config_.token = std::move(token);

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Milvus vector search node created (uri=%s, collection=%s, dim=%d, threshold=%.2f, metric=%s, source=%s)",
            node_name.c_str(), config_.uri.c_str(), config_.collection_name.c_str(),
            config_.dimension, similarity_threshold_, config_.metric_type.c_str(),
            target_source_ == VectorTargetSource::FACE_TARGETS ? "FACE" : "OBJECT"));
        this->initialized();
    }

    cvedix_milvus_vector_search_node::~cvedix_milvus_vector_search_node() {
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Shutting down (searches=%lu, matches=%lu, avg_ms=%.1f)",
            node_name.c_str(), total_searches_.load(), total_matches_.load(), avg_search_ms_.load()));
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            if (client_) { client_->Disconnect(); client_.reset(); }
        }
        connected_ = false;
        deinitialized();
    }

    // ═══════════════════════════════════════════════════════════════
    //  Connection Management
    // ═══════════════════════════════════════════════════════════════

    bool cvedix_milvus_vector_search_node::ensureConnection() {
        if (connected_.load()) return true;
        auto now = std::chrono::steady_clock::now();
        if (connect_failures_ > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_connect_attempt_).count();
            if (elapsed < reconnect_interval_sec_) return false;
        }
        last_connect_attempt_ = now;
        try {
            if (!client_) client_ = milvus::MilvusClientV2::Create();
            milvus::ConnectParam connect_param{config_.uri};
            if (!config_.token.empty()) connect_param = milvus::ConnectParam{config_.uri, config_.token};
            auto status = client_->Connect(connect_param);
            if (!status.IsOk()) {
                connect_failures_++;
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Connect failed: %s (#%d)",
                    node_name.c_str(), status.Message().c_str(), connect_failures_));
                return false;
            }
            connected_ = true; connect_failures_ = 0;
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Connected to Milvus at %s", node_name.c_str(), config_.uri.c_str()));
            return true;
        } catch (const std::exception& e) {
            connect_failures_++;
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception connecting: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_vector_search_node::ensureCollection() {
        if (collection_ready_.load()) return true;
        if (!ensureConnection()) return false;
        try {
            milvus::HasCollectionRequest has_req;
            has_req.SetCollectionName(config_.collection_name);
            milvus::HasCollectionResponse has_res;
            auto status = client_->HasCollection(has_req, has_res);
            bool exists = has_res.Has();
            if (status.IsOk() && exists) {
                milvus::LoadCollectionRequest load_req;
                load_req.SetCollectionName(config_.collection_name);
                status = client_->LoadCollection(load_req);
                if (status.IsOk()) {
                    collection_ready_ = true;
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Collection '%s' loaded", node_name.c_str(), config_.collection_name.c_str()));
                    return true;
                }
            }
            if (!exists) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Collection '%s' not found, creating...", node_name.c_str(), config_.collection_name.c_str()));
                return createCollectionInternal();
            }
            return false;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception checking collection: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_vector_search_node::createCollectionInternal() {
        if (!client_ || !connected_.load()) return false;
        try {
            auto status = client_->CreateCollection(
                milvus::CreateSimpleCollectionRequest()
                    .WithCollectionName(config_.collection_name)
                    .WithPrimaryFieldName(schema_.primary_field)
                    .WithVectorFieldName(schema_.vector_field)
                    .WithDimension(config_.dimension)
                    .WithAutoID(true));
            if (!status.IsOk()) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to create collection: %s", node_name.c_str(), status.Message().c_str()));
                return false;
            }
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Collection '%s' created (dim=%d)", node_name.c_str(), config_.collection_name.c_str(), config_.dimension));
            if (!createIndexInternal()) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Collection created but index creation failed", node_name.c_str()));
            }
            milvus::LoadCollectionRequest load_req;
            load_req.SetCollectionName(config_.collection_name);
            status = client_->LoadCollection(load_req);
            if (status.IsOk()) { collection_ready_ = true; return true; }
            return false;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception creating collection: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    bool cvedix_milvus_vector_search_node::createIndexInternal() {
        if (!client_ || !connected_.load()) return false;
        try {
            milvus::IndexDesc index_desc;
            index_desc.SetFieldName(schema_.vector_field);
            index_desc.SetIndexName("");
            index_desc.SetMetricType(milvus::MetricType::L2);
            index_desc.SetIndexType(milvus::IndexType::HNSW);
            if (config_.index_type == "HNSW") {
                index_desc.AddExtraParam("M", std::to_string(config_.hnsw_m));
                index_desc.AddExtraParam("efConstruction", std::to_string(config_.hnsw_ef_construction));
            }
            milvus::CreateIndexRequest idx_req;
            idx_req.SetCollectionName(config_.collection_name);
            idx_req.AddIndex(std::move(index_desc));
            auto status = client_->CreateIndex(idx_req);
            if (!status.IsOk()) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Failed to create index: %s", node_name.c_str(), status.Message().c_str()));
                return false;
            }
            CVEDIX_INFO(cvedix_utils::string_format("[%s] %s index created on '%s' (M=%d, ef=%d)",
                node_name.c_str(), config_.index_type.c_str(), schema_.vector_field.c_str(), config_.hnsw_m, config_.hnsw_ef_construction));
            return true;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception creating index: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  UUID Generation
    // ═══════════════════════════════════════════════════════════════

    std::string cvedix_milvus_vector_search_node::generateUUID() {
        std::uniform_int_distribution<int> dist(0, 15);
        const char* v = "0123456789ABCDEF";
        std::string res = "Guest-";
        for (int i = 0; i < 4; i++) res += v[dist(rng_)];
        return res;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Core Pipeline: handle_frame_meta (supports both target sources)
    // ═══════════════════════════════════════════════════════════════

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_milvus_vector_search_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
    {
        if (!meta) return meta;

        std::vector<size_t> target_indices;
        std::vector<std::vector<float>> query_embeddings;

        if (target_source_ == VectorTargetSource::FACE_TARGETS) {
            if (meta->face_targets.empty()) return meta;
            for (size_t i = 0; i < meta->face_targets.size(); i++) {
                auto& face = meta->face_targets[i];
                if (face && !face->embeddings.empty()) {
                    if (!face->identify.empty() && face->identify != "Unknown") continue;
                    target_indices.push_back(i);
                    query_embeddings.push_back(face->embeddings);
                }
            }
        } else {
            if (meta->targets.empty()) return meta;
            for (size_t i = 0; i < meta->targets.size(); i++) {
                auto& target = meta->targets[i];
                if (target && !target->embeddings.empty()) {
                    if (!target->identify.empty() && target->identify != "Unknown") continue;
                    target_indices.push_back(i);
                    query_embeddings.push_back(target->embeddings);
                }
            }
        }

        if (query_embeddings.empty()) return meta;

        std::unique_lock<std::mutex> lock(client_mutex_);
        if (!ensureCollection()) { total_failures_++; return meta; }

        auto start = std::chrono::steady_clock::now();
        auto results = batchSearch(query_embeddings);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;

        total_searches_++;
        avg_search_ms_ = (avg_search_ms_.load() * (total_searches_.load() - 1) + elapsed) / total_searches_.load();

        for (size_t i = 0; i < results.size() && i < target_indices.size(); i++) {
            auto& result = results[i];
            if (result.id >= 0 && result.score >= similarity_threshold_) {
                if (target_source_ == VectorTargetSource::FACE_TARGETS) {
                    meta->face_targets[target_indices[i]]->identify = result.label;
                    meta->face_targets[target_indices[i]]->identify_score = result.score;
                } else {
                    meta->targets[target_indices[i]]->identify = result.label;
                    meta->targets[target_indices[i]]->identify_score = result.score;
                }
                total_matches_++;
            } else if (auto_register_enabled_) {
                std::string new_uuid = generateUUID();
                lock.unlock();
                int64_t new_id = registerEntity(query_embeddings[i], new_uuid, "{\"auto_registered\":true}");
                lock.lock();
                if (new_id >= 0) {
                    if (target_source_ == VectorTargetSource::FACE_TARGETS) {
                        meta->face_targets[target_indices[i]]->identify = new_uuid;
                        meta->face_targets[target_indices[i]]->identify_score = 1.0f;
                    } else {
                        meta->targets[target_indices[i]]->identify = new_uuid;
                        meta->targets[target_indices[i]]->identify_score = 1.0f;
                    }
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] Auto-registered: %s (id=%ld)", node_name.c_str(), new_uuid.c_str(), new_id));
                }
            }
        }
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_milvus_vector_search_node::handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Batch Search (uses schema config for field names)
    // ═══════════════════════════════════════════════════════════════

    std::vector<milvus_search_result>
    cvedix_milvus_vector_search_node::batchSearch(
        const std::vector<std::vector<float>>& embeddings)
    {
        std::vector<milvus_search_result> results(embeddings.size());
        if (embeddings.empty() || !client_ || !connected_.load()) return results;

        try {
            // Build output fields from schema
            std::set<std::string> output_fields = {schema_.label_field, schema_.metadata_field};
            for (auto& ef : schema_.extra_fields) output_fields.insert(ef.first);

            milvus::SearchRequest req;
            req.WithCollectionName(config_.collection_name)
               .WithAnnsField(schema_.vector_field)
               .WithLimit(top_k_)
               .WithOutputFields(std::move(output_fields))
               .AddExtraParam("ef", std::to_string(config_.hnsw_ef_search));
            for (const auto& emb : embeddings) req.AddFloatVector(emb);

            milvus::SearchResponse response;
            auto status = client_->Search(req, response);
            if (!status.IsOk()) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Search failed: %s", node_name.c_str(), status.Message().c_str()));
                if (status.Message().find("connect") != std::string::npos ||
                    status.Message().find("unavailable") != std::string::npos) {
                    connected_ = false; collection_ready_ = false;
                }
                return results;
            }

            auto search_results = response.Results().Results();
            for (size_t i = 0; i < search_results.size() && i < results.size(); i++) {
                if (!search_results[i].Scores().empty()) {
                    auto& top = search_results[i];
                    results[i].id = top.Ids().IntIDArray()[0];
                    results[i].score = top.Scores()[0];

                    auto label_f = std::dynamic_pointer_cast<milvus::VarCharFieldData>(top.OutputField(schema_.label_field));
                    if (label_f && !label_f->Data().empty()) results[i].label = label_f->Data()[0];

                    auto meta_f = std::dynamic_pointer_cast<milvus::VarCharFieldData>(top.OutputField(schema_.metadata_field));
                    if (meta_f && !meta_f->Data().empty()) results[i].metadata = meta_f->Data()[0];

                    for (auto& ef : schema_.extra_fields) {
                        auto ef_data = std::dynamic_pointer_cast<milvus::VarCharFieldData>(top.OutputField(ef.first));
                        if (ef_data && !ef_data->Data().empty()) results[i].extra_fields[ef.first] = ef_data->Data()[0];
                    }
                }
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception in batch search: %s", node_name.c_str(), e.what()));
            connected_ = false; collection_ready_ = false;
        }
        return results;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Registration API
    // ═══════════════════════════════════════════════════════════════

    int64_t cvedix_milvus_vector_search_node::registerEntity(
        const std::vector<float>& embedding, const std::string& label, const std::string& metadata) {
        auto ids = registerEntities({embedding}, {label}, {metadata});
        return ids.empty() ? -1 : ids[0];
    }

    std::vector<int64_t> cvedix_milvus_vector_search_node::registerEntities(
        const std::vector<std::vector<float>>& embeddings,
        const std::vector<std::string>& labels,
        const std::vector<std::string>& metadatas)
    {
        std::vector<int64_t> result_ids(embeddings.size(), -1);
        if (embeddings.empty() || embeddings.size() != labels.size()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] registerEntities: invalid input (embeddings=%zu, labels=%zu)",
                node_name.c_str(), embeddings.size(), labels.size()));
            return result_ids;
        }
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureCollection()) { CVEDIX_ERROR(cvedix_utils::string_format("[%s] Cannot register: Milvus unavailable", node_name.c_str())); return result_ids; }

        try {
            for (size_t i = 0; i < embeddings.size(); i++) {
                if (static_cast<int>(embeddings[i].size()) != config_.dimension) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] Dimension mismatch for '%s': got %zu, expected %d",
                        node_name.c_str(), labels[i].c_str(), embeddings[i].size(), config_.dimension));
                    return result_ids;
                }
            }
            std::vector<milvus::FieldDataPtr> fields;
            fields.push_back(std::make_shared<milvus::VarCharFieldData>(schema_.label_field, std::vector<std::string>(labels)));
            std::vector<std::string> meta_values(embeddings.size());
            for (size_t i = 0; i < embeddings.size(); i++)
                meta_values[i] = (i < metadatas.size()) ? metadatas[i] : "";
            fields.push_back(std::make_shared<milvus::VarCharFieldData>(schema_.metadata_field, meta_values));
            int64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            fields.push_back(std::make_shared<milvus::Int64FieldData>(schema_.timestamp_field, std::vector<int64_t>(embeddings.size(), now)));
            fields.push_back(std::make_shared<milvus::FloatVecFieldData>(schema_.vector_field, embeddings));

            milvus::InsertResponse insert_response;
            milvus::InsertRequest insert_req;
            insert_req.SetCollectionName(config_.collection_name);
            insert_req.SetColumnsData(std::move(fields));
            auto status = client_->Insert(insert_req, insert_response);
            if (!status.IsOk()) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Insert failed: %s", node_name.c_str(), status.Message().c_str()));
                return result_ids;
            }
            auto& ids = insert_response.Results().IdArray().IntIDArray();
            for (size_t i = 0; i < ids.size() && i < result_ids.size(); i++) result_ids[i] = ids[i];
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Registered %zu entities (first: '%s' -> id=%ld)",
                node_name.c_str(), embeddings.size(), labels[0].c_str(), result_ids[0]));
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception registering: %s", node_name.c_str(), e.what()));
        }
        return result_ids;
    }

    // ═══════════════════════════════════════════════════════════════
    //  Deletion API
    // ═══════════════════════════════════════════════════════════════

    bool cvedix_milvus_vector_search_node::deleteEntity(int64_t entity_id) {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection() || !client_) return false;
        try {
            std::string filter = schema_.primary_field + " == " + std::to_string(entity_id);
            milvus::DeleteRequest del_req; del_req.SetCollectionName(config_.collection_name); del_req.SetFilter(filter);
            milvus::DeleteResponse del_res;
            auto status = client_->Delete(del_req, del_res);
            if (status.IsOk()) { CVEDIX_INFO(cvedix_utils::string_format("[%s] Deleted id=%ld", node_name.c_str(), entity_id)); return true; }
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Delete id=%ld failed: %s", node_name.c_str(), entity_id, status.Message().c_str()));
            return false;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception deleting: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    int cvedix_milvus_vector_search_node::deleteEntityByLabel(const std::string& label) {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection() || !client_) return 0;
        try {
            std::string filter = schema_.label_field + " == \"" + label + "\"";
            milvus::DeleteRequest del_req; del_req.SetCollectionName(config_.collection_name); del_req.SetFilter(filter);
            milvus::DeleteResponse del_res;
            auto status = client_->Delete(del_req, del_res);
            if (status.IsOk()) { CVEDIX_INFO(cvedix_utils::string_format("[%s] Deleted by label='%s'", node_name.c_str(), label.c_str())); return 1; }
            return 0;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception deleting by label: %s", node_name.c_str(), e.what()));
            return 0;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Query & Collection Management
    // ═══════════════════════════════════════════════════════════════

    std::vector<milvus_search_result> cvedix_milvus_vector_search_node::searchEntity(
        const std::vector<float>& embedding, int top_k) {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureCollection()) return {};
        int k = (top_k > 0) ? top_k : top_k_;
        int saved = top_k_; top_k_ = k;
        auto results = batchSearch({embedding});
        top_k_ = saved;
        return results;
    }

    int64_t cvedix_milvus_vector_search_node::getDatabaseSize() {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection() || !client_) return -1;
        try {
            milvus::GetCollectionStatsResponse stat_res;
            milvus::GetCollectionStatsRequest stat_req;
            stat_req.SetCollectionName(config_.collection_name);
            auto status = client_->GetCollectionStats(stat_req, stat_res);
            if (status.IsOk()) return stat_res.Stats().RowCount();
            return -1;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception getting DB size: %s", node_name.c_str(), e.what()));
            return -1;
        }
    }

    bool cvedix_milvus_vector_search_node::createCollection() {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection()) return false;
        return createCollectionInternal();
    }

    bool cvedix_milvus_vector_search_node::dropCollection() {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (!ensureConnection() || !client_) return false;
        try {
            milvus::DropCollectionRequest drop_req;
            drop_req.SetCollectionName(config_.collection_name);
            auto status = client_->DropCollection(drop_req);
            if (status.IsOk()) {
                collection_ready_ = false;
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Dropped collection '%s'", node_name.c_str(), config_.collection_name.c_str()));
                return true;
            }
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to drop: %s", node_name.c_str(), status.Message().c_str()));
            return false;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception dropping: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    std::string cvedix_milvus_vector_search_node::to_string() {
        std::ostringstream oss;
        oss << "cvedix_milvus_vector_search_node {"
            << " name=" << node_name
            << ", uri=" << config_.uri
            << ", collection=" << config_.collection_name
            << ", dim=" << config_.dimension
            << ", threshold=" << similarity_threshold_
            << ", source=" << (target_source_ == VectorTargetSource::FACE_TARGETS ? "FACE" : "OBJECT")
            << ", label_field=" << schema_.label_field
            << ", connected=" << (connected_.load() ? "yes" : "no")
            << ", searches=" << total_searches_.load()
            << ", matches=" << total_matches_.load()
            << ", avg_ms=" << avg_search_ms_.load()
            << " }";
        return oss.str();
    }

}

#endif // CVEDIX_WITH_MILVUS
