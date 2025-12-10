#include <lcxx/identifiers/hardware.hpp>
#include <iostream>
#include <string>

/**
 * @brief CVEDIX Hardware ID Tool
 * 
 * Gets the hardware ID of the current machine for license binding.
 * 
 * Usage:
 *   get_hardware_id
 *   get_hardware_id --verbose
 * 
 * Output:
 *   Hardware hash (base64 encoded) suitable for use with --hardware-id option
 */
int main(int argc, char* argv[]) {
    bool verbose = false;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CVEDIX Hardware ID Tool" << std::endl;
            std::cout << std::endl;
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -v, --verbose    Show detailed hardware information" << std::endl;
            std::cout << "  -h, --help       Show this help message" << std::endl;
            std::cout << std::endl;
            std::cout << "Example:" << std::endl;
            std::cout << "  " << argv[0] << std::endl;
            std::cout << "  " << argv[0] << " --verbose" << std::endl;
            return 0;
        }
    }

    try {
        // Get hardware identifier
        auto hw_id = lcxx::experimental::identifiers::hardware(
            lcxx::experimental::identifiers::hw_ident_strat::all
        );

        if (verbose) {
            std::cout << "Hardware Information:" << std::endl;
            std::cout << "====================" << std::endl;
            std::cout << "Hardware Hash: " << hw_id.hash << std::endl;
            std::cout << std::endl;
            std::cout << "Hardware Details:" << std::endl;
            std::cout << hw_id.source_text << std::endl;
            std::cout << std::endl;
            std::cout << "Usage:" << std::endl;
            std::cout << "  ./license_generator private_key.pem license.lic 2025-12-31 --hardware-id " << hw_id.hash << std::endl;
        } else {
            // Just output the hash (for scripting)
            std::cout << hw_id.hash << std::endl;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to get hardware ID: " << e.what() << std::endl;
        std::cerr << "Hardware binding may not be supported on this platform." << std::endl;
        return 1;
    }
}

