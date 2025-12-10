# Hướng dẫn sử dụng licensecxx

**Version:** 2025.0.1.3  
**Last Updated:** December 9, 2025

---

## Tổng quan

licensecxx là một thư viện C++20 hiện đại để tạo và xác thực license files sử dụng RSA signature. License files được lưu dưới dạng JSON với cryptographic signature.

---

## Bước 1: Tạo RSA Key Pair

Trước khi tạo license, bạn cần có một cặp RSA key (private key và public key).

### Sử dụng OpenSSL

```bash
# Tạo private key (1024-bit RSA)
openssl genrsa -out private_key.pem 1024

# Tạo public key từ private key
openssl rsa -in private_key.pem -pubout -out public_key.pem
```

**Lưu ý quan trọng:**
- **Private key**: Giữ bí mật, chỉ dùng để tạo license. Không bao giờ phân phối với ứng dụng.
- **Public key**: Phân phối cùng với ứng dụng để verify license.

### Kiểm tra keys

```bash
# Xem thông tin private key
openssl rsa -in private_key.pem -text -noout

# Xem thông tin public key
openssl rsa -in public_key.pem -pubin -text -noout
```

---

## Bước 2: Tạo License Generator Program

Tạo một chương trình C++ để generate license files:

### File: `tools/license_generator.cpp`

```cpp
#include <lcxx/lcxx.hpp>
#include <iostream>
#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <private_key.pem> <output.lic> <expiration_date>" << std::endl;
        std::cerr << "Example: " << argv[0] << " private_key.pem license.lic 2025-12-31" << std::endl;
        return 1;
    }

    std::string private_key_path = argv[1];
    std::string output_file = argv[2];
    std::string expiration = argv[3];

    try {
        // Tạo license object
        lcxx::license license;

        // Thêm các thông tin vào license
        license.push_content("application", "CVEDIX_AI_RUNTIME");
        license.push_content("expiration", expiration);
        license.push_content("features", "tensorrt,rknn,insightface");
        license.push_content("version", "2025.0.1.3");
        
        // (Tùy chọn) Thêm hardware binding
        // license.push_content("hardware", lcxx::identifiers::hardware().hash);
        // license.push_content("os", lcxx::identifiers::os().hash);

        // Load private key từ file
        auto private_key = lcxx::crypto::load_key(
            std::filesystem::path(private_key_path),
            lcxx::crypto::key_type::private_key
        );

        // Generate signed license file
        lcxx::to_json(license, std::filesystem::path(output_file), private_key);

        std::cout << "License generated successfully: " << output_file << std::endl;
        std::cout << "Expiration date: " << expiration << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error generating license: " << e.what() << std::endl;
        return 1;
    }
}
```

### Build License Generator

Thêm vào `tools/CMakeLists.txt`:

```cmake
add_executable(license_generator license_generator.cpp)
target_link_libraries(license_generator PRIVATE lcxx::lcxx)
```

Hoặc build thủ công:

```bash
g++ -std=c++20 license_generator.cpp -o license_generator \
    -I../third_party/licensecxx/modules \
    $(pkg-config --cflags --libs openssl) \
    -L../build/libs -llcxx
```

### Sử dụng License Generator

```bash
# Generate license với expiration date
./license_generator private_key.pem license.lic 2025-12-31

# Kiểm tra license file đã tạo
cat license.lic
```

**Output (license.lic):**
```json
{
  "content": {
    "application": "CVEDIX_AI_RUNTIME",
    "expiration": "2025-12-31",
    "features": "tensorrt,rknn,insightface",
    "version": "2025.0.1.3"
  },
  "signature": "<base64-encoded-RSA-signature>"
}
```

---

## Bước 3: Verify License trong Code

License verification đã được tích hợp vào `cvedix_license_manager`. Dưới đây là cách nó hoạt động:

### Implementation hiện tại

File `utils/license/cvedix_license_manager.cpp` đã implement verification:

```cpp
bool cvedix_license_manager::do_check_license() {
    // Load public key
    auto public_key = lcxx::crypto::load_key(
        std::filesystem::path(key_file),
        lcxx::crypto::key_type::public_key
    );
    
    // Load license from JSON file
    auto [license, signature] = lcxx::from_json(std::filesystem::path(license_file));
    
    // Verify license signature
    bool valid = lcxx::verify_license(license, signature, public_key);
    
    return valid;
}
```

### Sử dụng trong Code

```cpp
#include "cvedix/utils/license/cvedix_license_manager.h"

// Check license status
auto& license_mgr = cvedix_utils::cvedix_license_manager::get_instance();

if (!license_mgr.check_license()) {
    std::cerr << "License validation failed!" << std::endl;
    return -1;
}

// License is valid, proceed...
```

---

## Bước 4: Cấu hình License Files

### License File Location

License manager tự động tìm license file theo thứ tự:

1. **Environment variable**: `CVEDIX_LICENSE_PATH`
2. **Current directory**: `./license.lic`
3. **System directories**: `/opt/cvedix/license.lic` hoặc `/etc/cvedix/license.lic`

### Public Key Location

Public key được tìm theo thứ tự:

1. **Environment variable**: `CVEDIX_LICENSE_PUBLIC_KEY`
2. **Current directory**: `./public_key.pem`
3. **System directories**: `/opt/cvedix/public_key.pem` hoặc `/etc/cvedix/public_key.pem`

### Ví dụ Setup

```bash
# Copy files vào system directory
sudo mkdir -p /opt/cvedix
sudo cp license.lic /opt/cvedix/
sudo cp public_key.pem /opt/cvedix/
sudo chmod 644 /opt/cvedix/license.lic
sudo chmod 644 /opt/cvedix/public_key.pem

# Hoặc sử dụng environment variables
export CVEDIX_LICENSE_PATH=/path/to/license.lic
export CVEDIX_LICENSE_PUBLIC_KEY=/path/to/public_key.pem
```

---

## Bước 5: Advanced Usage

### Hardware Binding (Tùy chọn)

Để bind license với hardware cụ thể:

```cpp
// Trong license generator
license.push_content("hardware", lcxx::identifiers::hardware().hash);
license.push_content("os", lcxx::identifiers::os().hash);

// Trong license verifier (nếu cần)
if (auto hw_hash = license.get("hardware")) {
    if (!lcxx::identifiers::verify(lcxx::identifiers::hw_ident_strat::all, *hw_hash)) {
        // Hardware không khớp
        return false;
    }
}
```

**Lưu ý**: Hardware binding hiện chỉ hỗ trợ tốt trên Linux.

### Custom License Content

Bạn có thể thêm bất kỳ key-value pairs nào vào license:

```cpp
license.push_content("customer_id", "CUSTOMER_123");
license.push_content("max_users", "100");
license.push_content("trial", "false");
license.push_content("activation_date", "2025-01-01");
```

### Đọc License Content

```cpp
auto [license, signature] = lcxx::from_json(std::filesystem::path("license.lic"));

// Đọc các giá trị
if (auto expiration = license.get("expiration")) {
    std::cout << "Expires: " << *expiration << std::endl;
}

if (auto features = license.get("features")) {
    std::cout << "Features: " << *features << std::endl;
}
```

---

## Bước 6: Kiểm tra License Expiration

Bạn có thể thêm logic kiểm tra expiration date:

```cpp
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

bool check_expiration(const lcxx::license& license) {
    if (auto expiration_str = license.get("expiration")) {
        std::tm tm = {};
        std::istringstream ss(*expiration_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        
        if (ss.fail()) {
            return false; // Invalid date format
        }
        
        auto expiration_time = std::mktime(&tm);
        auto current_time = std::time(nullptr);
        
        return current_time < expiration_time;
    }
    return true; // No expiration date set
}
```

---

## Troubleshooting

### Lỗi: "Public key file not found"

**Nguyên nhân**: Public key không được tìm thấy.

**Giải pháp**:
```bash
# Đảm bảo public_key.pem tồn tại
ls -la public_key.pem

# Hoặc set environment variable
export CVEDIX_LICENSE_PUBLIC_KEY=/path/to/public_key.pem
```

### Lỗi: "License signature verification failed"

**Nguyên nhân**: 
- License file bị sửa đổi
- Public key không khớp với private key dùng để tạo license
- License file bị corrupt

**Giải pháp**:
1. Kiểm tra license file không bị sửa đổi
2. Đảm bảo public key đúng
3. Tạo lại license file nếu cần

### Lỗi: "Cannot open license file"

**Nguyên nhân**: File không tồn tại hoặc không có quyền đọc.

**Giải pháp**:
```bash
# Kiểm tra file tồn tại
ls -la license.lic

# Kiểm tra quyền
chmod 644 license.lic
```

---

## Best Practices

1. **Bảo mật Private Key**:
   - Không bao giờ commit private key vào git
   - Lưu private key ở nơi an toàn
   - Chỉ dùng private key trên máy build license

2. **Phân phối Public Key**:
   - Public key có thể phân phối công khai
   - Có thể embed vào ứng dụng hoặc đặt ở system directory

3. **License File Format**:
   - Không sửa đổi license file thủ công
   - Luôn tạo license mới bằng license generator
   - Backup license files quan trọng

4. **Expiration Management**:
   - Set expiration date hợp lý
   - Monitor expiration và renew trước khi hết hạn
   - Log expiration warnings

5. **Error Handling**:
   - Luôn check license trước khi sử dụng protected features
   - Provide clear error messages cho users
   - Log license validation failures

---

## Ví dụ hoàn chỉnh

### Generate License Script

```bash
#!/bin/bash
# generate_license.sh

PRIVATE_KEY="./private_key.pem"
OUTPUT="./license.lic"
EXPIRATION="2025-12-31"

if [ ! -f "$PRIVATE_KEY" ]; then
    echo "Error: Private key not found: $PRIVATE_KEY"
    exit 1
fi

./license_generator "$PRIVATE_KEY" "$OUTPUT" "$EXPIRATION"

if [ $? -eq 0 ]; then
    echo "License generated: $OUTPUT"
    echo "Expiration: $EXPIRATION"
else
    echo "Failed to generate license"
    exit 1
fi
```

### Verify License trong Application

```cpp
#include "cvedix/utils/license/cvedix_license_manager.h"
#include "cvedix/utils/logger/cvedix_logger.h"

int main() {
    CVEDIX_LOGGER_INIT();
    
    // Check license
    auto& license_mgr = cvedix_utils::cvedix_license_manager::get_instance();
    
    if (!license_mgr.check_license()) {
        CVEDIX_ERROR("License validation failed!");
        CVEDIX_ERROR("Please contact support for a valid license.");
        return 1;
    }
    
    CVEDIX_INFO("License validated successfully");
    
    // Continue with protected features...
    // ...
    
    return 0;
}
```

---

## References

- [licensecxx GitHub](https://github.com/felixjulianheitmann/licensecxx)
- [licensecxx Documentation](https://felixjulianheitmann.github.io/licensecxx/)
- [OpenSSL Documentation](https://www.openssl.org/docs/)

---

**Last Updated:** December 9, 2025  
**Version:** 2025.0.1.3

