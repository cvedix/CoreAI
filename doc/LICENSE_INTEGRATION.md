# License Integration Guide - CVEDIX AI Runtime SDK

**Version:** 2025.0.1.3  
**Last Updated:** December 7, 2025

---

## Overview

CVEDIX AI Runtime SDK integrates with [licensecxx](https://github.com/felixjulianheitmann/licensecxx) to protect premium features:

- **TensorRT nodes**: All nodes with prefix `cvedix_trt_*`
- **RKNN nodes**: All nodes with prefix `cvedix_rknn_*`
- **InsightFace nodes**: `cvedix_insight_face_recognition_node` and `cvedix_trt_insight_face_recognition_node`

When license checking is enabled (`CVEDIX_WITH_LICENSE=ON`), these features require a valid license file to function. Other features (ONNX with OpenCV DNN, basic nodes) continue to work without a license.

**Note**: This SDK requires C++20 support (licensecxx requires C++20).

---

## Building with License Support

### Prerequisites

1. **licensecxx library**: Clone as submodule (required)
2. **OpenSSL >= 3.0**: Required by licensecxx
3. **C++20 compiler**: GCC 10+, Clang 10+, or MSVC 2019+

### Option 1: Using Submodule (Required)

```bash
# Add licensecxx as submodule
cd /path/to/core_ai_runtime
git submodule add https://github.com/felixjulianheitmann/licensecxx.git third_party/licensecxx
git submodule update --init --recursive

# Build with license support
mkdir build && cd build
cmake .. -DCVEDIX_WITH_LICENSE=ON
make -j$(nproc)
```

### Option 2: Development Mode (No License Check)

```bash
# Build without license checking (all features work)
mkdir build && cd build
cmake .. -DCVEDIX_WITH_LICENSE=OFF
make -j$(nproc)
```

---

## License File Configuration

### License File Location

The license manager searches for license files in the following order:

1. **Environment variable**: `CVEDIX_LICENSE_PATH` (if set)
2. **Current directory**: `./license.lic`
3. **System directories**: `/opt/cvedix/license.lic` or `/etc/cvedix/license.lic`

### Public Key Location

The license manager searches for public key files in the following order:

1. **Environment variable**: `CVEDIX_LICENSE_PUBLIC_KEY` (if set)
2. **Current directory**: `./public_key.pem`
3. **System directories**: `/opt/cvedix/public_key.pem` or `/etc/cvedix/public_key.pem`

### Setting License and Key Paths

```bash
# Using environment variables
export CVEDIX_LICENSE_PATH=/path/to/your/license.lic
export CVEDIX_LICENSE_PUBLIC_KEY=/path/to/your/public_key.pem

# Or place files in current directory
cp /path/to/license.lic ./license.lic
cp /path/to/public_key.pem ./public_key.pem
```

---

## Generating Licenses

### Step 1: Generate RSA Key Pair

First, generate a private/public key pair using OpenSSL:

```bash
# Generate private key (1024-bit RSA)
openssl genrsa -out private_key.pem 1024

# Extract public key from private key
openssl rsa -in private_key.pem -pubout -out public_key.pem
```

**Security Note**: Keep the private key secure and never distribute it. Only distribute the public key with your application.

### Step 2: Generate License File

Create a simple C++ program to generate licenses using licensecxx API:

```cpp
#include <lcxx/lcxx.hpp>
#include <iostream>

int main() {
    // Create license object
    lcxx::license license;
    
    // Add license content
    license.push_content("application", "CVEDIX_AI_RUNTIME");
    license.push_content("expiration", "2025-12-31");
    license.push_content("features", "tensorrt,rknn,insightface");
    
    // Load private key
    auto private_key = lcxx::crypto::load_key(
        std::filesystem::path("private_key.pem"),
        lcxx::crypto::key_type::private_key
    );
    
    // Generate signed license file
    lcxx::to_json(license, "license.lic", private_key);
    
    std::cout << "License generated: license.lic" << std::endl;
    return 0;
}
```

Compile and run:

```bash
# Compile license generator (requires licensecxx)
g++ -std=c++20 license_generator.cpp -o license_generator $(pkg-config --cflags --libs licensecxx)

# Generate license
./license_generator
```

**Note**: The license file is a JSON file with cryptographic signature. Do not modify it manually.

---

## Using Protected Features

### Example: TensorRT Node

```cpp
#include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/utils/logger/cvedix_logger.h"

int main() {
    CVEDIX_LOGGER_INIT();
    
    try {
        // This will throw exception if license is invalid
        auto detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>(
            "vehicle_detector",
            "./models/vehicle_det.trt"
        );
        
        // Use detector...
        
    } catch (const std::runtime_error& e) {
        CVEDIX_ERROR(std::string("Failed to create TensorRT node: ") + e.what());
        return 1;
    }
    
    return 0;
}
```

### Example: RKNN Node

```cpp
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"

int main() {
    try {
        // This will throw exception if license is invalid
        auto detector = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
            "rknn_detector",
            "./models/yolov8n.rknn",
            0.5f,  // score_threshold
            0.5f,  // nms_threshold
            640,   // input_width
            640,   // input_height
            80     // num_classes
        );
        
        // Use detector...
        
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed to create RKNN node: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### Example: InsightFace Node

```cpp
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"

int main() {
    try {
        // This will throw exception if license is invalid
        auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
            "face_recognizer",
            "./models/arcface_r100.onnx",
            112,  // input_width
            112,  // input_height
            true  // enable_alignment
        );
        
        // Use recognizer...
        
    } catch (const std::runtime_error& e) {
        std::cerr << "Failed to create InsightFace node: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

## License Manager API

### Check License

```cpp
#include "cvedix/utils/license/cvedix_license_manager.h"

// Check if license is valid
bool is_valid = cvedix_utils::cvedix_license_manager::get_instance().check_license();

// Get cached license status
bool is_licensed = cvedix_utils::cvedix_license_manager::get_instance().is_licensed();

// Get license file path
std::string path = cvedix_utils::cvedix_license_manager::get_instance().get_license_path();
```

---

## Troubleshooting

### Error: "TensorRT features require a valid license"

**Cause**: License file not found or invalid.

**Solutions**:
1. Check license file exists at expected location
2. Verify license file is valid (not corrupted)
3. Check license expiration date
4. Ensure `CVEDIX_LICENSE_PATH` environment variable is set correctly (if using)

### Error: "licensecxx not found" during build

**Cause**: licensecxx library not available as submodule.

**Solutions**:
1. Add licensecxx as submodule: `git submodule add https://github.com/felixjulianheitmann/licensecxx.git third_party/licensecxx`
2. Initialize submodule: `git submodule update --init --recursive`
3. Build without license: `cmake .. -DCVEDIX_WITH_LICENSE=OFF`

### Error: "OpenSSL >= 3.0 not found" during build

**Cause**: licensecxx requires OpenSSL >= 3.0.

**Solutions**:
```bash
# Ubuntu/Debian (check version first)
openssl version

# If version < 3.0, install OpenSSL 3.0:
# Ubuntu 22.04+ has OpenSSL 3.0 by default
sudo apt-get install libssl-dev

# Fedora/RHEL
sudo dnf install openssl-devel

# Arch Linux
sudo pacman -S openssl
```

### Error: "Public key file not found"

**Cause**: Public key file required for license verification.

**Solutions**:
1. Ensure `public_key.pem` exists in one of the default locations
2. Set `CVEDIX_LICENSE_PUBLIC_KEY` environment variable
3. Place `public_key.pem` in current directory or `/opt/cvedix/` or `/etc/cvedix/`

### Protected Features Work Without License

**Cause**: `CVEDIX_WITH_LICENSE=OFF` during build.

**Solution**: Rebuild with `-DCVEDIX_WITH_LICENSE=ON` to enable license checking.

---

## Implementation Notes

### License Check Timing

- License is checked **once** when the first protected node is created
- Result is cached for performance (singleton pattern)
- Thread-safe implementation

### Error Handling

- Invalid or missing license throws `std::runtime_error`
- Applications should catch exceptions and handle gracefully
- Error messages indicate which feature requires license

### Development Mode

- When `CVEDIX_WITH_LICENSE=OFF`, no license checks are performed
- All features work normally (development/testing)
- Production builds should use `CVEDIX_WITH_LICENSE=ON`

---

## License File Format

License files are generated by licensecxx and are JSON files containing:

- Key-value pairs (application identifier, expiration date, features, etc.)
- Cryptographic signature (RSA signature of the content)

**Example license.lic structure**:
```json
{
  "content": {
    "application": "CVEDIX_AI_RUNTIME",
    "expiration": "2025-12-31",
    "features": "tensorrt,rknn,insightface"
  },
  "signature": "<base64-encoded-RSA-signature>"
}
```

**Do not modify license files manually** - they are cryptographically signed. Any modification will invalidate the signature.

---

## Security Considerations

1. **License File Protection**: Store license files securely, restrict read permissions
2. **Hardware Binding**: Use hardware-bound licenses for production deployments
3. **Expiration Monitoring**: Monitor license expiration and renew before expiry
4. **Network Validation**: Consider online license validation for enhanced security

---

## References

- [licensecxx GitHub Repository](https://github.com/felixjulianheitmann/licensecxx)
- [licensecxx Documentation](https://felixjulianheitmann.github.io/licensecxx/)
- [OpenSSL Documentation](https://www.openssl.org/docs/)

---

## Support

For license-related issues:

- **Email**: support@cvedix.com
- **License Generation**: Contact your CVEDIX representative
- **Technical Issues**: Check licensecxx documentation or GitHub issues

---

**Last Updated:** December 7, 2025  
**Version:** 2025.0.1.3

