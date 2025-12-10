#pragma once

#ifdef CVEDIX_WITH_LICENSE
#include <string>
#include <mutex>
#include <memory>
#include <lcxx/lcxx.hpp>

namespace cvedix_utils {
    /**
     * @brief License manager wrapper for CVEDIX AI Runtime SDK
     * 
     * Singleton class that manages license checking for protected features:
     * - TensorRT nodes
     * - RKNN nodes
     * - InsightFace nodes
     * 
     * Uses licensecxx library for license verification (JSON format with RSA signature).
     * License is checked once and cached for performance.
     * 
     * The license file can contain an embedded public key (recommended),
     * or use a separate public_key.pem file for backward compatibility.
     */
    class cvedix_license_manager {
    private:
        cvedix_license_manager();
        ~cvedix_license_manager();
        
        // Non-copyable
        cvedix_license_manager(const cvedix_license_manager&) = delete;
        cvedix_license_manager& operator=(const cvedix_license_manager&) = delete;
        
        bool license_valid = false;
        bool checked = false;
        std::mutex check_mutex;
        std::string license_path;
        std::string public_key_path;
        
        bool do_check_license();
        std::string find_license_path();  // Private helper to find license path
        std::string find_public_key_path();  // Private helper to find public key path
        
    public:
        /**
         * @brief Get singleton instance
         */
        static cvedix_license_manager& get_instance();
        
        /**
         * @brief Check if license is valid
         * @return true if license is valid, false otherwise
         */
        bool check_license();
        
        /**
         * @brief Get license status (cached)
         * @return true if license is valid, false otherwise
         */
        bool is_licensed();
        
        /**
         * @brief Get license file path
         * @return Path to license file
         */
        std::string get_license_path();
    };
}
#endif // CVEDIX_WITH_LICENSE

