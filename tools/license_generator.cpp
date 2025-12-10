#include <lcxx/lcxx.hpp>
#include <lcxx/identifiers/hardware.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <memory>

/**
 * @brief Extract public key from private key using OpenSSL
 * 
 * @param private_key_path Path to private key file
 * @return Public key in PEM format as string
 */
std::string extract_public_key(const std::string& private_key_path) {
    std::string cmd = "openssl rsa -in \"" + private_key_path + "\" -pubout 2>/dev/null";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to execute openssl command");
    }
    
    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0 || result.empty()) {
        throw std::runtime_error("Failed to extract public key from private key");
    }
    
    return result;
}

/**
 * @brief CVEDIX License Generator
 * 
 * Generates license files for CVEDIX AI Runtime SDK using licensecxx.
 * The generated license file includes the public key embedded in it,
 * so only one file (license.lic) is needed for distribution.
 * 
 * Usage:
 *   license_generator <private_key.pem> <output.lic> <expiration_date> [features] [--bind-hardware] [--hardware-id ID]
 * 
 * Example:
 *   license_generator private_key.pem license.lic 2025-12-31 "tensorrt,rknn,insightface"
 *   license_generator private_key.pem license.lic 2025-12-31 --bind-hardware
 *   license_generator private_key.pem license.lic 2025-12-31 --hardware-id abc123xyz789...
 */
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "CVEDIX License Generator" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <private_key.pem> <output.lic> <expiration_date> [features] [--bind-hardware] [--hardware-id ID]" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Arguments:" << std::endl;
        std::cerr << "  private_key.pem  Path to RSA private key file" << std::endl;
        std::cerr << "  output.lic       Output license file path" << std::endl;
        std::cerr << "  expiration_date Expiration date (YYYY-MM-DD format)" << std::endl;
        std::cerr << "  features         (Optional) Comma-separated feature list" << std::endl;
        std::cerr << "                    Default: tensorrt,rknn,insightface" << std::endl;
        std::cerr << "  --bind-hardware  (Optional) Bind license to current machine hardware" << std::endl;
        std::cerr << "  --hardware-id ID (Optional) Bind license to specific hardware ID" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example:" << std::endl;
        std::cerr << "  " << argv[0] << " private_key.pem license.lic 2025-12-31" << std::endl;
        std::cerr << "  " << argv[0] << " private_key.pem license.lic 2025-12-31 \"tensorrt,rknn\"" << std::endl;
        std::cerr << "  " << argv[0] << " private_key.pem license.lic 2025-12-31 --bind-hardware" << std::endl;
        std::cerr << "  " << argv[0] << " private_key.pem license.lic 2025-12-31 --hardware-id abc123..." << std::endl;
        return 1;
    }

    std::string private_key_path = argv[1];
    std::string output_file = argv[2];
    std::string expiration = argv[3];
    std::string features = "tensorrt,rknn,insightface";
    bool bind_hardware = false;
    std::string hardware_id = "";

    // Parse optional arguments
    for (int i = 4; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--bind-hardware") {
            bind_hardware = true;
        } else if (arg == "--hardware-id" && i + 1 < argc) {
            hardware_id = argv[++i];
        } else if (features == "tensorrt,rknn,insightface" && arg.find("--") != 0) {
            // First non-option argument is features
            features = arg;
        }
    }

    // Validate inputs
    if (!std::filesystem::exists(private_key_path)) {
        std::cerr << "Error: Private key file not found: " << private_key_path << std::endl;
        return 1;
    }

    // Validate expiration date format (basic check)
    if (expiration.length() != 10 || expiration[4] != '-' || expiration[7] != '-') {
        std::cerr << "Error: Invalid expiration date format. Use YYYY-MM-DD (e.g., 2025-12-31)" << std::endl;
        return 1;
    }

    try {
        std::cout << "Generating license..." << std::endl;
        std::cout << "  Private key: " << private_key_path << std::endl;
        std::cout << "  Output file: " << output_file << std::endl;
        std::cout << "  Expiration:  " << expiration << std::endl;
        std::cout << "  Features:    " << features << std::endl;

        // Create license object
        lcxx::license license;

        // Add license content
        license.push_content("application", "CVEDIX_AI_RUNTIME");
        license.push_content("expiration", expiration);
        license.push_content("features", features);
        license.push_content("version", "2025.0.1.3");
        license.push_content("generated_date", std::to_string(std::time(nullptr)));

        // Extract public key from private key and embed it in license
        std::cout << "  Extracting public key..." << std::endl;
        std::string public_key_pem = extract_public_key(private_key_path);
        license.push_content("public_key", public_key_pem);

        // Add hardware binding if requested
        if (bind_hardware) {
            try {
                std::cout << "  Binding to current hardware..." << std::endl;
                auto hw_id = lcxx::experimental::identifiers::hardware(
                    lcxx::experimental::identifiers::hw_ident_strat::all
                );
                license.push_content("hardware", hw_id.hash);
                std::cout << "    Hardware ID: " << hw_id.hash << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to get hardware ID: " << e.what() << std::endl;
                std::cerr << "License will be generated without hardware binding." << std::endl;
            }
        } else if (!hardware_id.empty()) {
            std::cout << "  Binding to specified hardware ID..." << std::endl;
            license.push_content("hardware", hardware_id);
            std::cout << "    Hardware ID: " << hardware_id << std::endl;
        }

        // Load private key from file
        auto private_key = lcxx::crypto::load_key(
            std::filesystem::path(private_key_path),
            lcxx::crypto::key_type::private_key
        );

        // Generate signed license file
        lcxx::to_json(license, std::filesystem::path(output_file), private_key);

        std::cout << std::endl;
        std::cout << "✓ License generated successfully!" << std::endl;
        std::cout << "  File: " << output_file << std::endl;
        std::cout << "  Note: Public key is embedded in license file" << std::endl;
        if (bind_hardware || !hardware_id.empty()) {
            std::cout << "  Note: License is bound to hardware (cannot be transferred to other machines)" << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Next steps:" << std::endl;
        std::cout << "  1. Copy " << output_file << " to target system" << std::endl;
        std::cout << "  2. Set CVEDIX_LICENSE_PATH environment variable (optional)" << std::endl;
        std::cout << "     Default locations: ./license.lic, /opt/cvedix/license.lic, /etc/cvedix/license.lic" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::endl;
        std::cerr << "✗ Error generating license: " << e.what() << std::endl;
        return 1;
    }
}

