/**
 * @file cvedix_license_node.cpp
 * @brief Implementation of license management node
 */

#include "cvedix_license_node.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <algorithm>

#ifdef CVEDIX_WITH_LICENSE
#include <lcxx/lcxx.hpp>
#include <lcxx/identifiers/hardware.hpp>
#endif

namespace fs = std::filesystem;

namespace cvedix_nodes {

// ========================================
// Constructor
// ========================================

cvedix_license_node::cvedix_license_node(
    const std::string& node_name,
    const std::string& license_path
) : node_name_(node_name), license_path_(license_path) {
    
    if (license_path_.empty()) {
        license_path_ = find_license_path();
    }
    
    // Pre-load hardware info
    load_hardware_info();
    
    // Pre-load license info if available
    if (!license_path_.empty()) {
        load_license_info();
    }
    
    initialized_ = true;
}

// ========================================
// Helper Methods
// ========================================

std::string cvedix_license_node::find_license_path() {
    // Search order for license file
    std::vector<std::string> search_paths = {
        "./license.lic",
        "./cvedix_data/license.lic",
        "/opt/cvedix/license.lic",
        "/etc/cvedix/license.lic"
    };
    
    // Check environment variable first
    const char* env_path = std::getenv("CVEDIX_LICENSE_PATH");
    if (env_path && fs::exists(env_path)) {
        return env_path;
    }
    
    // Search default paths
    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    
    return "";
}

bool cvedix_license_node::load_hardware_info() {
#ifdef CVEDIX_WITH_LICENSE
    try {
        auto hw_id = lcxx::experimental::identifiers::hardware(
            lcxx::experimental::identifiers::hw_ident_strat::all
        );
        
        cached_hardware_info_.hash = hw_id.hash;
        cached_hardware_info_.source_text = hw_id.source_text;
        
        // Parse source_text for individual components (if available)
        // Format depends on lcxx implementation
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[" << node_name_ << "] Failed to get hardware info: " << e.what() << std::endl;
        return false;
    }
#else
    cached_hardware_info_.hash = "HARDWARE_ID_NOT_AVAILABLE";
    cached_hardware_info_.source_text = "License module not compiled";
    return false;
#endif
}

bool cvedix_license_node::load_license_info() {
#ifdef CVEDIX_WITH_LICENSE
    if (license_path_.empty() || !fs::exists(license_path_)) {
        cached_license_info_.valid = false;
        cached_license_info_.error_message = "License file not found";
        return false;
    }
    
    try {
        // Load license from JSON file - returns (license, signature) tuple
        auto [license, signature] = lcxx::from_json(fs::path(license_path_));
        
        // Try to extract public key from license content
        lcxx::crypto::rsa_key_t public_key;
        bool public_key_loaded = false;
        
        auto public_key_opt = license.get("public_key");
        if (public_key_opt.has_value()) {
            try {
                std::string public_key_pem = public_key_opt.value();
                public_key = lcxx::crypto::load_key(
                    public_key_pem,
                    lcxx::crypto::key_type::public_key
                );
                public_key_loaded = true;
            } catch (const std::exception& e) {
                // Failed to load embedded public key
            }
        }
        
        if (!public_key_loaded) {
            cached_license_info_.valid = false;
            cached_license_info_.error_message = "Cannot load public key from license";
            return false;
        }
        
        // Verify license signature
        bool valid = lcxx::verify_license(license, signature, public_key);
        
        if (!valid) {
            cached_license_info_.valid = false;
            cached_license_info_.error_message = "License signature verification failed";
            return false;
        }
        
        // Extract license info using correct API: license.get(key) returns std::optional
        cached_license_info_.valid = true;
        
        auto exp_opt = license.get("expiration");
        if (exp_opt.has_value()) {
            cached_license_info_.expiration = exp_opt.value();
        }
        
        auto feat_opt = license.get("features");
        if (feat_opt.has_value()) {
            cached_license_info_.features = feat_opt.value();
        }
        
        auto ver_opt = license.get("version");
        if (ver_opt.has_value()) {
            cached_license_info_.version = ver_opt.value();
        }
        
        auto app_opt = license.get("application");
        if (app_opt.has_value()) {
            cached_license_info_.application = app_opt.value();
        }
        
        auto hw_opt = license.get("hardware");
        if (hw_opt.has_value()) {
            cached_license_info_.hardware_id = hw_opt.value();
            cached_license_info_.hardware_bound = true;
            
            // Verify hardware binding
            try {
                bool hardware_match = lcxx::experimental::identifiers::verify(
                    lcxx::experimental::identifiers::hw_ident_strat::all,
                    cached_license_info_.hardware_id
                );
                
                if (!hardware_match) {
                    cached_license_info_.valid = false;
                    cached_license_info_.error_message = "Hardware mismatch";
                    return false;
                }
            } catch (const std::exception& e) {
                cached_license_info_.valid = false;
                cached_license_info_.error_message = std::string("Hardware verification failed: ") + e.what();
                return false;
            }
        }
        
        // Check expiration
        if (!cached_license_info_.expiration.empty()) {
            // Parse expiration date (YYYY-MM-DD)
            std::tm tm = {};
            std::istringstream ss(cached_license_info_.expiration);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            
            std::time_t exp_time = std::mktime(&tm);
            std::time_t now = std::time(nullptr);
            
            if (now > exp_time) {
                cached_license_info_.valid = false;
                cached_license_info_.error_message = "License expired";
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        cached_license_info_.valid = false;
        cached_license_info_.error_message = std::string("License validation failed: ") + e.what();
        return false;
    }
#else
    cached_license_info_.valid = false;
    cached_license_info_.error_message = "License module not compiled";
    return false;
#endif
}

// ========================================
// Hardware ID Functions
// ========================================

std::string cvedix_license_node::get_hardware_id() {
    return cached_hardware_info_.hash;
}

HardwareInfo cvedix_license_node::get_hardware_info() {
    return cached_hardware_info_;
}

void cvedix_license_node::print_hardware_info() {
    std::cout << "========================================\n";
    std::cout << "🖥️  Hardware Information\n";
    std::cout << "========================================\n";
    std::cout << "Hardware ID: " << cached_hardware_info_.hash << "\n";
    if (!cached_hardware_info_.source_text.empty()) {
        std::cout << "\nDetails:\n" << cached_hardware_info_.source_text << "\n";
    }
    std::cout << "========================================\n";
}

// ========================================
// License Functions
// ========================================

bool cvedix_license_node::is_licensed() {
    return cached_license_info_.valid;
}

bool cvedix_license_node::check_license() {
    load_license_info();
    return cached_license_info_.valid;
}

LicenseInfo cvedix_license_node::get_license_info() {
    return cached_license_info_;
}

std::string cvedix_license_node::get_license_path() {
    return license_path_;
}

bool cvedix_license_node::has_feature(const std::string& feature_name) {
    if (!cached_license_info_.valid) {
        return false;
    }
    
    // Parse features (comma-separated)
    std::string features = cached_license_info_.features;
    std::transform(features.begin(), features.end(), features.begin(), ::tolower);
    
    std::string feature_lower = feature_name;
    std::transform(feature_lower.begin(), feature_lower.end(), feature_lower.begin(), ::tolower);
    
    return features.find(feature_lower) != std::string::npos;
}

std::vector<std::string> cvedix_license_node::get_licensed_features() {
    std::vector<std::string> result;
    
    if (!cached_license_info_.valid) {
        return result;
    }
    
    std::stringstream ss(cached_license_info_.features);
    std::string feature;
    
    while (std::getline(ss, feature, ',')) {
        // Trim whitespace
        feature.erase(0, feature.find_first_not_of(" \t"));
        feature.erase(feature.find_last_not_of(" \t") + 1);
        
        if (!feature.empty()) {
            result.push_back(feature);
        }
    }
    
    return result;
}

void cvedix_license_node::print_license_info() {
    std::cout << "========================================\n";
    std::cout << "📜 License Information\n";
    std::cout << "========================================\n";
    
    if (license_path_.empty()) {
        std::cout << "❌ No license file found\n";
    } else {
        std::cout << "File: " << license_path_ << "\n";
        
        if (cached_license_info_.valid) {
            std::cout << "Status: ✅ Valid\n";
            std::cout << "Application: " << cached_license_info_.application << "\n";
            std::cout << "Version: " << cached_license_info_.version << "\n";
            std::cout << "Expiration: " << cached_license_info_.expiration << "\n";
            std::cout << "Features: " << cached_license_info_.features << "\n";
            
            if (cached_license_info_.hardware_bound) {
                std::cout << "Hardware Bound: Yes\n";
                std::cout << "Hardware ID: " << cached_license_info_.hardware_id << "\n";
            } else {
                std::cout << "Hardware Bound: No (floating license)\n";
            }
        } else {
            std::cout << "Status: ❌ Invalid\n";
            std::cout << "Error: " << cached_license_info_.error_message << "\n";
        }
    }
    
    std::cout << "========================================\n";
}

// ========================================
// License Request Functions
// ========================================

std::string cvedix_license_node::create_license_request(const LicenseRequest& request) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"type\": \"license_request\",\n";
    json << "  \"hardware_id\": \"" << request.hardware_id << "\",\n";
    json << "  \"requested_features\": \"" << request.requested_features << "\",\n";
    json << "  \"requested_expiration\": \"" << request.requested_expiration << "\",\n";
    json << "  \"contact_email\": \"" << request.contact_email << "\",\n";
    json << "  \"company_name\": \"" << request.company_name << "\",\n";
    json << "  \"timestamp\": " << std::time(nullptr) << "\n";
    json << "}\n";
    
    return json.str();
}

std::string cvedix_license_node::create_simple_request() {
    LicenseRequest request;
    request.hardware_id = get_hardware_id();
    request.requested_features = "all";
    request.requested_expiration = "1-year";
    
    return create_license_request(request);
}

bool cvedix_license_node::save_license_request(
    const std::string& filepath,
    const LicenseRequest& request
) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[" << node_name_ << "] Cannot open file for writing: " << filepath << "\n";
            return false;
        }
        
        file << create_license_request(request);
        std::cout << "[" << node_name_ << "] License request saved to: " << filepath << "\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[" << node_name_ << "] Error saving request: " << e.what() << "\n";
        return false;
    }
}

// ========================================
// Utility Functions
// ========================================

void cvedix_license_node::print_all_info() {
    std::cout << "\n";
    print_hardware_info();
    std::cout << "\n";
    print_license_info();
    
    if (!cached_license_info_.valid && !cached_hardware_info_.hash.empty()) {
        std::cout << "\n💡 To request a license, run:\n";
        std::cout << "   Hardware ID: " << cached_hardware_info_.hash << "\n";
        std::cout << "\n   Use this ID to generate a license with license_generator tool.\n";
    }
}

} // namespace cvedix_nodes
