/**
 * @file cvedix_invalid_argument_error.h
 * @brief Exception for invalid function arguments
 */

#pragma once

#include <stdexcept>

namespace cvedix_excepts {

    /**
     * @brief Invalid argument exception
     */
    class cvedix_invalid_argument_error: public std::runtime_error {

    private:
        /* data */
    public:
        cvedix_invalid_argument_error(const std::string& what_arg);
        ~cvedix_invalid_argument_error();
    };
    
    inline cvedix_invalid_argument_error::cvedix_invalid_argument_error(const std::string& what_arg): std::runtime_error(what_arg) {
    }
    
    inline cvedix_invalid_argument_error::~cvedix_invalid_argument_error() {
    }
}