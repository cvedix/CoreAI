import re

file_path = '/home/cvedix/CVEDIX-AI/core/nodes/infers/cvedix_milvus_face_search_node.cpp'

with open(file_path, 'r') as f:
    code = f.read()

code = code.replace(
    'auto status = client_->HasCollection(config_.collection_name, exists);',
    '''milvus::HasCollectionRequest has_req;
            has_req.SetCollectionName(config_.collection_name);
            milvus::HasCollectionResponse has_res;
            auto status = client_->HasCollection(has_req, has_res);
            exists = has_res.Has();'''
)

code = code.replace(
    'status = client_->LoadCollection(config_.collection_name);',
    '''milvus::LoadCollectionRequest load_req;
                load_req.SetCollectionName(config_.collection_name);
                status = client_->LoadCollection(load_req);'''
)

code = code.replace(
    'milvus::IndexDesc index_desc("embedding", "", config_.metric_type);',
    '''milvus::IndexDesc index_desc;
            index_desc.SetFieldName("embedding");
            index_desc.SetIndexName("");
            index_desc.SetMetricType(milvus::MetricType::L2);'''
)

code = code.replace(
    'index_desc.SetIndexType(config_.index_type);',
    'index_desc.SetIndexType(milvus::IndexType::HNSW);'
)

code = code.replace(
    'auto status = client_->CreateIndex(config_.collection_name, index_desc);',
    '''milvus::CreateIndexRequest idx_req;
            idx_req.SetCollectionName(config_.collection_name);
            idx_req.AddIndex(std::move(index_desc));
            auto status = client_->CreateIndex(idx_req);'''
)

code = code.replace('.WithVectorFieldName("embedding")', '.WithAnnsField("embedding")')

old_search_results = '''if (!search_results[i].empty()) {
                    auto& top_match = search_results[i][0];  // Best match
                    results[i].id = top_match.Id();
                    results[i].score = top_match.Score();

                    // Extract scalar fields
                    auto name_field = top_match.OutputField("name");
                    if (name_field) {
                        results[i].name = name_field->StringValue();
                    }

                    auto meta_field = top_match.OutputField("metadata");
                    if (meta_field) {
                        results[i].metadata = meta_field->StringValue();
                    }
                }'''

new_search_results = '''if (!search_results[i].Scores().empty()) {
                    auto& top_match = search_results[i];  // Best match
                    results[i].id = top_match.Ids().IntIDArray()[0];
                    results[i].score = top_match.Scores()[0];

                    // Extract scalar fields
                    auto name_field = top_match.OutputField<milvus::VarCharFieldData>("name");
                    if (name_field && !name_field->DataAsString().empty()) {
                        results[i].name = name_field->DataAsString()[0];
                    }

                    auto meta_field = top_match.OutputField<milvus::VarCharFieldData>("metadata");
                    if (meta_field && !meta_field->DataAsString().empty()) {
                        results[i].metadata = meta_field->DataAsString()[0];
                    }
                }'''

code = code.replace(old_search_results, new_search_results)

code = code.replace('std::vector<milvus::FieldData> fields;', 'std::vector<milvus::FieldDataPtr> fields;')

code = code.replace(
    'fields.push_back(milvus::FieldData("name", name_values));',
    'fields.push_back(std::make_shared<milvus::VarCharFieldData>("name", name_values));'
)
code = code.replace(
    'fields.push_back(milvus::FieldData("metadata", meta_values));',
    'fields.push_back(std::make_shared<milvus::VarCharFieldData>("metadata", meta_values));'
)
code = code.replace(
    'fields.push_back(milvus::FieldData("created_at", timestamps));',
    'fields.push_back(std::make_shared<milvus::Int64FieldData>("created_at", timestamps));'
)
code = code.replace(
    'fields.push_back(milvus::FieldData("embedding", embeddings));',
    'fields.push_back(std::make_shared<milvus::FloatVecFieldData>("embedding", embeddings));'
)

old_insert = '''auto status = client_->Insert(
                config_.collection_name, fields, insert_response);'''

new_insert = '''milvus::InsertRequest insert_req;
            insert_req.SetCollectionName(config_.collection_name);
            insert_req.SetColumnsData(std::move(fields));
            auto status = client_->Insert(insert_req, insert_response);'''

code = code.replace(old_insert, new_insert)

code = code.replace('auto& ids = insert_response.Ids();', 'auto& ids = insert_response.InsertIdArray().IntIDArray();')

old_delete = 'auto status = client_->Delete(config_.collection_name, filter);'
new_delete = '''milvus::DeleteRequest del_req;
            del_req.SetCollectionName(config_.collection_name);
            del_req.SetFilter(filter);
            milvus::DeleteResponse del_res;
            auto status = client_->Delete(del_req, del_res);'''
code = code.replace(old_delete, new_delete)

old_stats = '''auto status = client_->GetCollectionStatistics(
                config_.collection_name, response);'''
new_stats = '''milvus::GetCollectionStatsRequest stat_req;
            stat_req.SetCollectionName(config_.collection_name);
            auto status = client_->GetCollectionStats(stat_req, response);'''
code = code.replace(old_stats, new_stats)

old_drop = 'auto status = client_->DropCollection(config_.collection_name);'
new_drop = '''milvus::DropCollectionRequest drop_req;
            drop_req.SetCollectionName(config_.collection_name);
            auto status = client_->DropCollection(drop_req);'''
code = code.replace(old_drop, new_drop)

with open(file_path, 'w') as f:
    f.write(code)
