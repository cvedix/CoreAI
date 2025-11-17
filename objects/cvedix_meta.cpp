
#include "cvedix_meta.h"
#include "../excepts/cvedix_invalid_argument_error.h"

namespace cvedix_objects {
    
    cvedix_meta::cvedix_meta(cvedix_meta_type meta_type, int channel_index): 
        meta_type(meta_type), 
        channel_index(channel_index) {
            create_time = std::chrono::system_clock::now();
    }

    cvedix_meta::~cvedix_meta() {

    }

    std::string cvedix_meta::get_traces_str() {
        return "";
    }

    std::string cvedix_meta::get_meta_str() {
        return "";
    }

/*
    void cvedix_meta::attach_trace(std::string node_name) {
        if (trace_table.count(node_name)) {
            return;
        }
        
        std::map<cvedix_meta_trace_field, std::any> new_trace_record {
            {cvedix_meta_trace_field::SEQUENCE, trace_table.size()},
            {cvedix_meta_trace_field::NODE_NAME, node_name},
            {cvedix_meta_trace_field::IN_TIME, -1},
            {cvedix_meta_trace_field::OUT_TIME, -1},
            {cvedix_meta_trace_field::TEXT_INFO, std::vector<std::string>{}}
        };

        // append to the end of table
        trace_table[node_name] = new_trace_record;
    }

    void cvedix_meta::update_trace(std::string node_name, cvedix_meta_trace_field trace_key, std::any trace_value) {
        if (trace_table.count(node_name)) {
            auto & trace_record = trace_table[node_name];
            assert(trace_record.count(trace_key));

            switch (trace_key) {
                case cvedix_meta_trace_field::SEQUENCE:
                case cvedix_meta_trace_field::NODE_NAME:
                case cvedix_meta_trace_field::IN_TIME:
                case cvedix_meta_trace_field::OUT_TIME: {
                    // replace directly
                    trace_record[trace_key] = trace_value;
                    break;
                }
                case cvedix_meta_trace_field::TEXT_INFO: {
                    // append to the end of vector
                    auto & trace_desc = std::any_cast<std::vector<std::string>&>(trace_record[trace_key]);
                    trace_desc.push_back(std::any_cast<std::string>(trace_value));
                    break;
                }
                default: {
                    throw cvedix_excepts::cvedix_invalid_argument_error("invalid trace_key for meta!");
                    break;
                }
            }
        }
    }*/
}