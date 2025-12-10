# Hướng dẫn tạo License Key cho CVEDIX AI Runtime SDK

**Version:** 2025.0.1.3  
**Last Updated:** December 9, 2025

---

## Tổng quan

Hướng dẫn này mô tả cách tạo license key (license file) để sử dụng với CVEDIX AI Runtime SDK. License được tạo bằng licensecxx library và sử dụng RSA signature để bảo mật.

---

## Phương pháp 1: Sử dụng Script (Khuyến nghị)

### Bước 1: Build License Generator

```bash
cd /home/cvedix/core_ai_runtime
mkdir -p build && cd build
cmake .. -DCVEDIX_WITH_LICENSE=ON
make license_generator
```

### Bước 2: Tạo License với Script

```bash
cd /home/cvedix/core_ai_runtime

# Tạo keys và license trong một lần
./tools/generate_license.sh --generate-keys --expiration 2025-12-31

# Hoặc sử dụng keys có sẵn
./tools/generate_license.sh \
    --private-key ./private_key.pem \
    --output ./license.lic \
    --expiration 2025-12-31 \
    --features "tensorrt,rknn,insightface"
```

### Các tùy chọn Script

```bash
# Xem help
./tools/generate_license.sh --help

# Tạo keys mới
./tools/generate_license.sh --generate-keys

# Tạo license với features cụ thể
./tools/generate_license.sh \
    --expiration 2025-12-31 \
    --features "tensorrt,rknn"
```

---

## Phương pháp 2: Sử dụng License Generator Tool trực tiếp

### Bước 1: Tạo RSA Key Pair

```bash
# Tạo private key (1024-bit RSA)
openssl genrsa -out private_key.pem 1024

# Tạo public key từ private key
openssl rsa -in private_key.pem -pubout -out public_key.pem
```

**Lưu ý quan trọng:**
- **Private key**: Giữ bí mật, chỉ dùng để tạo license. Không bao giờ phân phối.
- **Public key**: Phân phối cùng với ứng dụng để verify license.

### Bước 2: Build License Generator

```bash
cd /home/cvedix/core_ai_runtime
mkdir -p build && cd build
cmake .. -DCVEDIX_WITH_LICENSE=ON
make license_generator
```

### Bước 3: Generate License

```bash
# Basic usage
./license_generator private_key.pem license.lic 2025-12-31

# Với features cụ thể
./license_generator private_key.pem license.lic 2025-12-31 "tensorrt,rknn"

# Với hardware binding (bind với máy hiện tại)
./license_generator private_key.pem license.lic 2025-12-31 --bind-hardware

# Với hardware ID cụ thể (từ máy khác)
./license_generator private_key.pem license.lic 2025-12-31 --hardware-id abc123xyz789...
```

**Output**: File `license.lic` (JSON format) sẽ được tạo.

**Hardware Binding**: Nếu sử dụng `--bind-hardware`, license sẽ chỉ hoạt động trên máy đã bind. Để lấy hardware ID từ máy target, sử dụng tool `get_hardware_id`:
```bash
# Build hardware ID tool
make get_hardware_id

# Lấy hardware ID
./get_hardware_id
# Output: abc123xyz789...

# Hoặc xem chi tiết
./get_hardware_id --verbose
```

---

## Phương pháp 3: Tạo License bằng C++ Code

Nếu bạn muốn tích hợp license generation vào ứng dụng của mình:

### File: `my_license_generator.cpp`

```cpp
#include <lcxx/lcxx.hpp>
#include <iostream>
#include <filesystem>

int main() {
    try {
        // Tạo license object
        lcxx::license license;

        // Thêm các thông tin vào license
        license.push_content("application", "CVEDIX_AI_RUNTIME");
        license.push_content("expiration", "2025-12-31");
        license.push_content("features", "tensorrt,rknn,insightface");
        license.push_content("version", "2025.0.1.3");
        license.push_content("customer_id", "CUSTOMER_123");
        
        // (Tùy chọn) Thêm hardware binding
        // license.push_content("hardware", lcxx::identifiers::hardware().hash);
        // license.push_content("os", lcxx::identifiers::os().hash);

        // Load private key từ file
        auto private_key = lcxx::crypto::load_key(
            std::filesystem::path("private_key.pem"),
            lcxx::crypto::key_type::private_key
        );

        // Generate signed license file
        lcxx::to_json(license, std::filesystem::path("license.lic"), private_key);

        std::cout << "License generated: license.lic" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Build và chạy

```bash
g++ -std=c++20 my_license_generator.cpp -o my_license_generator \
    -I../third_party/licensecxx/modules \
    $(pkg-config --cflags --libs openssl) \
    -L../build/libs -llcxx

./my_license_generator
```

---

## Cấu trúc License File

License file là JSON với cấu trúc:

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

## Các trường License phổ biến

### Trường bắt buộc

- `application`: Tên ứng dụng (phải match với code)
- `expiration`: Ngày hết hạn (YYYY-MM-DD)

### Trường tùy chọn

- `features`: Danh sách features được phép (comma-separated)
- `version`: Version của SDK
- `customer_id`: ID khách hàng
- `max_users`: Số lượng users tối đa
- `trial`: "true" hoặc "false"
- `activation_date`: Ngày kích hoạt
- `hardware`: Hardware hash (cho hardware binding)
- `os`: OS hash (cho OS binding)

---

## Ví dụ tạo License cho các use cases

### 1. License đầy đủ tính năng

```bash
./tools/generate_license.sh \
    --expiration 2025-12-31 \
    --features "tensorrt,rknn,insightface"
```

### 2. License chỉ TensorRT

```bash
./tools/generate_license.sh \
    --expiration 2025-12-31 \
    --features "tensorrt"
```

### 3. License chỉ RKNN

```bash
./tools/generate_license.sh \
    --expiration 2025-12-31 \
    --features "rknn"
```

### 4. License trial (30 ngày)

```bash
# Tính toán expiration date (30 ngày từ hôm nay)
EXPIRATION=$(date -d "+30 days" +%Y-%m-%d)
./tools/generate_license.sh \
    --expiration "$EXPIRATION" \
    --features "tensorrt,rknn,insightface"
```

---

## Phân phối License và Public Key

### Cách 1: Copy vào system directories

```bash
# Copy license và public key
sudo mkdir -p /opt/cvedix
sudo cp license.lic /opt/cvedix/
sudo cp public_key.pem /opt/cvedix/
sudo chmod 644 /opt/cvedix/license.lic
sudo chmod 644 /opt/cvedix/public_key.pem
```

### Cách 2: Sử dụng Environment Variables

```bash
export CVEDIX_LICENSE_PATH=/path/to/license.lic
export CVEDIX_LICENSE_PUBLIC_KEY=/path/to/public_key.pem
```

### Cách 3: Đặt trong current directory

```bash
# Đơn giản nhất - đặt trong thư mục chạy ứng dụng
cp license.lic ./
cp public_key.pem ./
```

---

## Kiểm tra License

### Xem nội dung License (không verify signature)

```bash
cat license.lic | jq '.content'
```

### Verify License bằng OpenSSL (manual)

```bash
# Extract content từ license file
CONTENT=$(cat license.lic | jq -r '.content | to_entries | map("\(.key):\(.value)") | sort | join("")')

# Verify signature (cần implement logic này)
# Licensecxx sẽ tự động verify khi check_license() được gọi
```

### Test License trong Code

```cpp
#include "cvedix/utils/license/cvedix_license_manager.h"

auto& license_mgr = cvedix_utils::cvedix_license_manager::get_instance();
if (license_mgr.check_license()) {
    std::cout << "License is valid!" << std::endl;
} else {
    std::cout << "License is invalid!" << std::endl;
}
```

---

## Best Practices

### 1. Bảo mật Private Key

- Không commit private key vào git
- Lưu private key ở nơi an toàn (encrypted storage)
- Chỉ dùng private key trên máy build license
- Backup private key ở nhiều nơi an toàn

### 2. Quản lý Keys

```bash
# Tạo thư mục riêng cho keys
mkdir -p ~/cvedix_keys
cd ~/cvedix_keys

# Generate keys
openssl genrsa -out private_key.pem 1024
openssl rsa -in private_key.pem -pubout -out public_key.pem

# Set permissions
chmod 600 private_key.pem
chmod 644 public_key.pem
```

### 3. Version Control cho Keys

- **Private key**: Không bao giờ commit
- **Public key**: Có thể commit vào repository
- Tạo `.gitignore` để exclude private keys:

```gitignore
# License keys
*.pem
!public_key.pem
private_key*
```

### 4. Tạo nhiều License cho nhiều khách hàng

```bash
#!/bin/bash
# generate_multiple_licenses.sh

CUSTOMERS=("customer1" "customer2" "customer3")
EXPIRATION="2025-12-31"

for customer in "${CUSTOMERS[@]}"; do
    ./tools/generate_license.sh \
        --private-key ~/cvedix_keys/private_key.pem \
        --output "./licenses/${customer}_license.lic" \
        --expiration "$EXPIRATION" \
        --features "tensorrt,rknn,insightface"
    
    echo "Generated license for $customer"
done
```

---

## Troubleshooting

### Lỗi: "Private key file not found"

**Giải pháp**: Đảm bảo private key tồn tại hoặc generate mới:
```bash
./tools/generate_license.sh --generate-keys
```

### Lỗi: "Invalid expiration date format"

**Giải pháp**: Sử dụng format YYYY-MM-DD:
```bash
./tools/generate_license.sh --expiration 2025-12-31
```

### Lỗi: "license_generator executable not found"

**Giải pháp**: Build license generator trước:
```bash
cd build
cmake .. -DCVEDIX_WITH_LICENSE=ON
make license_generator
```

### License không verify được

**Kiểm tra**:
1. Public key đúng với private key dùng để tạo license
2. License file không bị sửa đổi
3. License file path đúng trong environment variable hoặc default locations

---

## Workflow đề xuất

### Development Environment

```bash
# 1. Generate development keys (một lần)
./tools/generate_license.sh --generate-keys

# 2. Tạo development license (không hết hạn sớm)
./tools/generate_license.sh \
    --expiration 2099-12-31 \
    --features "tensorrt,rknn,insightface"

# 3. Copy vào system directory
sudo cp license.lic /opt/cvedix/
sudo cp public_key.pem /opt/cvedix/
```

### Production Environment

```bash
# 1. Generate production keys (trên máy build riêng)
./tools/generate_license.sh --generate-keys

# 2. Tạo license cho từng khách hàng với expiration phù hợp
./tools/generate_license.sh \
    --expiration 2025-12-31 \
    --features "tensorrt,rknn,insightface"

# 3. Phân phối license và public key đến khách hàng
# (không bao giờ phân phối private key)
```

---

## Security Checklist

- [ ] Private key được lưu ở nơi an toàn
- [ ] Private key không được commit vào git
- [ ] Private key có permissions 600 (chỉ owner đọc/ghi)
- [ ] Public key có thể được phân phối công khai
- [ ] License files được backup định kỳ
- [ ] Expiration dates được monitor và renew trước khi hết hạn
- [ ] Mỗi khách hàng có license riêng (nếu cần)

---

## References

- [GUIDE_LICENSECXX_USAGE.md](GUIDE_LICENSECXX_USAGE.md) - Chi tiết về cách sử dụng licensecxx
- [LICENSE_INTEGRATION.md](LICENSE_INTEGRATION.md) - Tích hợp license vào code
- [licensecxx GitHub](https://github.com/felixjulianheitmann/licensecxx)

---

**Last Updated:** December 9, 2025  
**Version:** 2025.0.1.3

