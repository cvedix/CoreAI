# Hướng dẫn Reinstall Debian Package

## Vấn đề: Package không replace file cũ

Khi chạy `sudo apt-get install ./cvedix-ai-runtime-*.deb`, package sẽ:

### Trường hợp 1: Version mới hơn
- ✅ **Tự động replace/upgrade** các file cũ
- Không cần thao tác gì thêm

### Trường hợp 2: Cùng version
- ❌ **KHÔNG replace** file cũ
- apt sẽ báo: "package is already the newest version"
- Cần dùng `--reinstall` để force replace

### Trường hợp 3: Version cũ hơn  
- ❌ **Từ chối install** (downgrade)
- Cần dùng `--allow-downgrades` để force install

## Giải pháp

### Phương pháp 1: Sử dụng script tự động (Khuyến nghị)

```bash
# Script tự động detect và xử lý reinstall
./rebuild_and_reinstall_deb.sh
```

Script sẽ:
- Tự động detect version cũ
- Uninstall package cũ (nếu bạn đồng ý)
- Rebuild package mới
- Install với `--reinstall` nếu cùng version

### Phương pháp 2: Force reinstall thủ công

```bash
# Option 1: Uninstall trước, install sau
sudo apt-get remove cvedix-ai-runtime
sudo apt-get install ./cvedix-ai-runtime-*.deb

# Option 2: Dùng --reinstall (nếu cùng version)
sudo apt-get install --reinstall ./cvedix-ai-runtime-*.deb

# Option 3: Dùng dpkg với --force-overwrite
sudo dpkg -i --force-overwrite ./cvedix-ai-runtime-*.deb
```

### Phương pháp 3: Bump version (cho release mới)

Nếu cần release version mới:

```bash
# Sửa version trong CMakeLists.txt
# project(cvedix_instance_sdk VERSION 2025.0.1.4)

# Hoặc override khi build
cmake -DPROJECT_VERSION=2025.0.1.4 ..
make package
```

## Kiểm tra version

```bash
# Version đã install
dpkg -l | grep cvedix-ai-runtime

# Version trong package file
dpkg -I cvedix-ai-runtime-*.deb | grep Version

# So sánh
dpkg --compare-versions $(dpkg -I cvedix-ai-runtime-*.deb | grep Version | awk '{print $2}') gt $(dpkg -l cvedix-ai-runtime | grep ^ii | awk '{print $3}') && echo "Newer" || echo "Same or older"
```

## Verify sau khi install

```bash
# Kiểm tra file đã được update
grep "cvedix/third_party" /opt/cvedix/include/cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h

# Nếu vẫn thấy "../../../" thì package chưa được replace
# Cần chạy lại với --reinstall
```

## Lưu ý

- **Luôn backup** trước khi reinstall nếu có data quan trọng
- **Kiểm tra version** trước khi install để biết có cần reinstall không
- **Verify paths** sau khi install để đảm bảo file mới đã được replace

