#ifdef CVEDIX_WITH_LICENSE
#include "cvedix_license_manager.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <lcxx/lcxx.hpp>
#include <lcxx/identifiers/hardware.hpp>

namespace cvedix_utils {
    
    cvedix_license_manager::cvedix_license_manager() {
        license_path = find_license_path();
        public_key_path = find_public_key_path();
    }
    
    cvedix_license_manager::~cvedix_license_manager() {
    }
    
    cvedix_license_manager& cvedix_license_manager::get_instance() {
        static cvedix_license_manager instance;
        return instance;
    }
    
    std::string cvedix_license_manager::find_license_path() {
        // Check environment variable first
        const char* env_path = std::getenv("CVEDIX_LICENSE_PATH");
        if (env_path && std::strlen(env_path) > 0) {
            std::string path(env_path);
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        
        // Check default locations
        std::vector<std::string> default_paths = {
            "./license.lic",
            "/opt/cvedix/license.lic",
            "/etc/cvedix/license.lic"
        };
        
        for (const auto& path : default_paths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        
        // Return first default path even if it doesn't exist (for error reporting)
        return default_paths[0];
    }
    
    std::string cvedix_license_manager::find_public_key_path() {
        // Check environment variable first
        const char* env_key = std::getenv("CVEDIX_LICENSE_PUBLIC_KEY");
        if (env_key && std::strlen(env_key) > 0) {
            std::string path(env_key);
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        
        // Check default locations
        std::vector<std::string> default_paths = {
            "./public_key.pem",
            "/opt/cvedix/public_key.pem",
            "/etc/cvedix/public_key.pem"
        };
        
        for (const auto& path : default_paths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        
        // Return first default path even if it doesn't exist (for error reporting)
        return default_paths[0];
    }
    
    bool cvedix_license_manager::do_check_license() {
        std::string license_file = license_path;  // Use cached path
        
        // Check if license file exists
        if (!std::filesystem::exists(license_file)) {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[license] License file not found at: %s", license_file.c_str()));
            return false;
        }
        
        // Use licensecxx to validate license
        try {
            // Load license from JSON file
            auto [license, signature] = lcxx::from_json(std::filesystem::path(license_file));
            
            // Try to extract public key from license content first (new format)
            lcxx::crypto::rsa_key_t public_key;
            bool public_key_loaded = false;
            
            auto public_key_opt = license.get("public_key");
            if (public_key_opt.has_value()) {
                // New format: public key embedded in license
                try {
                    std::string public_key_pem = public_key_opt.value();
                    public_key = lcxx::crypto::load_key(
                        public_key_pem,
                        lcxx::crypto::key_type::public_key
                    );
                    public_key_loaded = true;
                    CVEDIX_INFO("[license] Using public key embedded in license file");
                } catch (const std::exception& e) {
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[license] Failed to load embedded public key: %s", e.what()));
                }
            }
            
            // Fallback to external public key file (backward compatibility)
            if (!public_key_loaded) {
                std::string key_file = public_key_path;
                if (std::filesystem::exists(key_file)) {
                    try {
                        public_key = lcxx::crypto::load_key(
                            std::filesystem::path(key_file),
                            lcxx::crypto::key_type::public_key
                        );
                        public_key_loaded = true;
                        CVEDIX_INFO(cvedix_utils::string_format(
                            "[license] Using external public key file: %s", key_file.c_str()));
                    } catch (const std::exception& e) {
                        CVEDIX_WARN(cvedix_utils::string_format(
                            "[license] Failed to load public key from file: %s", e.what()));
                    }
                } else {
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[license] Public key not found in license and external file not found at: %s", key_file.c_str()));
                }
            }
            
            if (!public_key_loaded) {
                CVEDIX_ERROR("[license] Cannot load public key from license or external file");
                return false;
            }
            
            // Verify license signature
            bool valid = lcxx::verify_license(license, signature, public_key);
            
            if (!valid) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[license] License signature verification failed for: %s", license_file.c_str()));
                return false;
            }
            
            // Check hardware binding if present in license
            auto hardware_opt = license.get("hardware");
            if (hardware_opt.has_value()) {
                std::string hardware_hash = hardware_opt.value();
                try {
                    bool hardware_match = lcxx::experimental::identifiers::verify(
                        lcxx::experimental::identifiers::hw_ident_strat::all,
                        hardware_hash
                    );
                    
                    if (!hardware_match) {
                        CVEDIX_WARN("[license] Hardware mismatch! License is bound to different hardware.");
                        CVEDIX_WARN(cvedix_utils::string_format(
                            "[license] Expected hardware hash: %s", hardware_hash.c_str()));
                        return false;
                    }
                    CVEDIX_INFO("[license] Hardware binding verified successfully");
                } catch (const std::exception& e) {
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[license] Failed to verify hardware binding: %s", e.what()));
                    CVEDIX_WARN("[license] Hardware verification failed, but signature is valid");
                    // For backward compatibility, we might want to allow this
                    // But for security, we should fail
                    return false;
                }
            }
            
            CVEDIX_INFO(cvedix_utils::string_format(
                "[license] License validated successfully from: %s", license_file.c_str()));
            return true;
            
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[license] Error checking license: %s", e.what()));
            return false;
        }
    }
    
    bool cvedix_license_manager::check_license() {
        std::lock_guard<std::mutex> lock(check_mutex);
        
        if (checked) {
            return license_valid;
        }
        
        license_valid = do_check_license();
        checked = true;
        
        if (license_valid) {
            CVEDIX_INFO("[license] License validation successful");
        } else {
            CVEDIX_WARN("[license] License validation failed or license not found");
        }
        
        return license_valid;
    }
    
    bool cvedix_license_manager::is_licensed() {
        std::lock_guard<std::mutex> lock(check_mutex);
        return license_valid;
    }
    
    std::string cvedix_license_manager::get_license_path() {
        return license_path;
    }
}

#endif // CVEDIX_WITH_LICENSE

