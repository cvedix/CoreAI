/**
 * @file license_info_sample.cpp
 * @brief Sample demonstrating cvedix_license_node usage
 * 
 * Shows how to:
 * - Get hardware ID for license binding
 * - Check license status
 * - Get licensed features
 * - Create license request
 * 
 * Usage:
 *   ./license_info_sample                    # Show all info
 *   ./license_info_sample --hardware         # Show hardware ID only
 *   ./license_info_sample --license          # Show license info only
 *   ./license_info_sample --request          # Generate license request
 */

#include "cvedix/nodes/common/cvedix_license_node.h"
#include <iostream>
#include <string>

void print_usage(const char* prog) {
    std::cout << "CVEDIX License Info Sample\n\n";
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --hardware, -hw    Show hardware ID only\n";
    std::cout << "  --license, -l      Show license info only\n";
    std::cout << "  --request, -r      Generate license request file\n";
    std::cout << "  --check, -c        Check if specific feature is licensed\n";
    std::cout << "  --help, -h         Show this help\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    std::string mode = "all";
    std::string feature_to_check;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--hardware" || arg == "-hw") {
            mode = "hardware";
        }
        else if (arg == "--license" || arg == "-l") {
            mode = "license";
        }
        else if (arg == "--request" || arg == "-r") {
            mode = "request";
        }
        else if ((arg == "--check" || arg == "-c") && i + 1 < argc) {
            mode = "check";
            feature_to_check = argv[++i];
        }
    }
    
    // Create license node
    auto license_node = std::make_shared<cvedix_nodes::cvedix_license_node>("license_sample");
    
    if (mode == "hardware") {
        // Only show hardware ID
        std::cout << license_node->get_hardware_id() << std::endl;
    }
    else if (mode == "license") {
        // Show license info
        license_node->print_license_info();
    }
    else if (mode == "request") {
        // Create license request
        std::cout << "========================================\n";
        std::cout << "📝 License Request Generator\n";
        std::cout << "========================================\n\n";
        
        cvedix_nodes::LicenseRequest request;
        request.hardware_id = license_node->get_hardware_id();
        request.requested_features = "tensorrt,rknn,face_recognition";
        request.requested_expiration = "1-year";
        
        std::cout << "Hardware ID: " << request.hardware_id << "\n\n";
        
        // Save to file
        std::string output_file = "license_request.json";
        if (license_node->save_license_request(output_file, request)) {
            std::cout << "\n✅ License request saved to: " << output_file << "\n";
            std::cout << "\nNext steps:\n";
            std::cout << "1. Send " << output_file << " to your license provider\n";
            std::cout << "2. Receive license.lic file\n";
            std::cout << "3. Place license.lic in current directory or /opt/cvedix/\n";
        }
        
        std::cout << "\n--- Request content ---\n";
        std::cout << license_node->create_license_request(request);
    }
    else if (mode == "check") {
        // Check specific feature
        bool has_feature = license_node->has_feature(feature_to_check);
        std::cout << "Feature '" << feature_to_check << "': ";
        if (has_feature) {
            std::cout << "✅ Licensed\n";
            return 0;
        } else {
            std::cout << "❌ Not licensed\n";
            return 1;
        }
    }
    else {
        // Show all info
        std::cout << "========================================\n";
        std::cout << "CVEDIX License Info Sample\n";
        std::cout << "========================================\n";
        
        license_node->print_all_info();
        
        // Show licensed features
        if (license_node->is_licensed()) {
            std::cout << "\n📋 Licensed Features:\n";
            auto features = license_node->get_licensed_features();
            for (const auto& f : features) {
                std::cout << "  ✅ " << f << "\n";
            }
        }
    }
    
    return 0;
}
