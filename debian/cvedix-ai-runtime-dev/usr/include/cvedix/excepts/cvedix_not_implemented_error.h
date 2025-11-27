
#pragma once

#include <stdexcept>

namespace cvedix_excepts {
    // not implemented error
    class cvedix_not_implemented_error: public std::runtime_error {
    private:
        /* data */
    public:
        cvedix_not_implemented_error(const std::string& what_arg);
        ~cvedix_not_implemented_error();
    };
    
    inline cvedix_not_implemented_error::cvedix_not_implemented_error(const std::string& what_arg): std::runtime_error(what_arg) {
    }
    
    inline cvedix_not_implemented_error::~cvedix_not_implemented_error() {
    }
    
}