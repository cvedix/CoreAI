/**
 * @file cvedix_invalid_calling_error.h
 * @brief Exception for invalid method call sequences
 */

#pragma once

#include <stdexcept>

namespace cvedix_excepts {

    /**
     * @brief Invalid calling sequence exception
     */
    class cvedix_invalid_calling_error: public std::runtime_error {

    private:
        /* data */
    public:
        cvedix_invalid_calling_error(const std::string& what_arg);
        ~cvedix_invalid_calling_error();
    };
    
    inline cvedix_invalid_calling_error::cvedix_invalid_calling_error(const std::string& what_arg): std::runtime_error(what_arg) {
    }
    
    inline cvedix_invalid_calling_error::~cvedix_invalid_calling_error() {
    }

}