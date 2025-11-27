#pragma once

#include <stdexcept>

namespace cvedix_excepts {

    class cvedix_invalid_pipeline_error: public std::runtime_error {
    private:
        /* data */
    public:
        cvedix_invalid_pipeline_error(const std::string& what_arg);
        ~cvedix_invalid_pipeline_error();
    };
    
    inline cvedix_invalid_pipeline_error::cvedix_invalid_pipeline_error(const std::string& what_arg): std::runtime_error(what_arg) {
    }
    
    inline cvedix_invalid_pipeline_error::~cvedix_invalid_pipeline_error() {
    }

}