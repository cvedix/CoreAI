# CVEDIX Tools

## License Generator

Tool để tạo license files cho CVEDIX AI Runtime SDK.

### Build

License generator và hardware ID tool được build tự động khi `CVEDIX_WITH_LICENSE=ON`:

```bash
cd build
cmake .. -DCVEDIX_WITH_LICENSE=ON
make license_generator get_hardware_id
```

### Sử dụng

#### Bước 1: Tạo RSA Key Pair

```bash
# Tạo private key
openssl genrsa -out private_key.pem 1024

# Tạo public key
openssl rsa -in private_key.pem -pubout -out public_key.pem
```

#### Bước 2: Generate License

```bash
# Basic usage
./license_generator private_key.pem license.lic 2025-12-31

# Với features cụ thể
./license_generator private_key.pem license.lic 2025-12-31 "tensorrt,rknn"

# Với hardware binding (bind với máy hiện tại)
./license_generator private_key.pem license.lic 2025-12-31 --bind-hardware

# Với hardware ID cụ thể
HW_ID=$(./get_hardware_id)
./license_generator private_key.pem license.lic 2025-12-31 --hardware-id "$HW_ID"
```

#### Bước 3: Phân phối License

```bash
# Copy license và public key đến target system
cp license.lic /opt/cvedix/
cp public_key.pem /opt/cvedix/

# Hoặc sử dụng environment variables
export CVEDIX_LICENSE_PATH=/path/to/license.lic
export CVEDIX_LICENSE_PUBLIC_KEY=/path/to/public_key.pem
```

### Xem thêm

Chi tiết đầy đủ: [GUIDE_LICENSECXX_USAGE.md](../doc/GUIDE_LICENSECXX_USAGE.md)

