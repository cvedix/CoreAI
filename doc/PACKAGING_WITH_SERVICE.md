# Hướng dẫn đóng gói Package kèm Service Configuration

## Tổng quan

Dự án này đã được cấu hình để đóng gói thành Debian package (.deb) kèm theo cấu hình systemd service để chạy CVEDIX AI Runtime như một service hệ thống.

## Các file đã được thêm vào

### 1. Service Files
- `debian/cvedix-ai-runtime.service`: Systemd service unit file
- `debian/cvedix-service-wrapper.sh`: Wrapper script để chạy sample applications
- `debian/cvedix-service.conf.example`: File cấu hình mẫu

### 2. Installation Scripts
- `debian/cvedix-ai-runtime.postinst`: Post-installation script (tạo user, directories, etc.)
- `debian/cvedix-ai-runtime.prerm`: Pre-removal script (dừng service trước khi gỡ)

### 3. Documentation
- `debian/README_SERVICE.md`: Tài liệu hướng dẫn sử dụng service

## Build Package

### Phương pháp 1: Sử dụng debuild (Khuyến nghị)

```bash
cd /home/ubuntu/core_ai_runtime
./debian/build_deb.sh --clean --release
```

Hoặc với samples:
```bash
./debian/build_deb.sh --clean --release --samples
```

### Phương pháp 2: Sử dụng dpkg-buildpackage trực tiếp

```bash
cd /home/ubuntu/core_ai_runtime
dpkg-buildpackage -b -us -uc
```

## Cài đặt Package

Sau khi build thành công, các file .deb sẽ được tạo trong thư mục cha:

```bash
# Cài đặt package
sudo dpkg -i ../cvedix-ai-runtime_*.deb

# Hoặc sử dụng apt
sudo apt-get install ./../cvedix-ai-runtime_*.deb
```

## Cấu hình và Sử dụng Service

### 1. Cấu hình Service

Chỉnh sửa file cấu hình:
```bash
sudo nano /etc/cvedix/service.conf
```

Ví dụ:
```bash
CVEDIX_SAMPLE="1-1-1_sample"
CVEDIX_WORK_DIR="/opt/cvedix"
```

### 2. Đảm bảo cvedix_data có sẵn

```bash
# Copy cvedix_data vào /opt/cvedix nếu chưa có
sudo cp -r /path/to/cvedix_data /opt/cvedix/
sudo chown -R cvedix:cvedix /opt/cvedix
```

### 3. Khởi động Service

```bash
# Khởi động service
sudo systemctl start cvedix-ai-runtime

# Kích hoạt để tự động khởi động khi boot
sudo systemctl enable cvedix-ai-runtime

# Kiểm tra trạng thái
sudo systemctl status cvedix-ai-runtime
```

### 4. Xem Logs

```bash
# Xem logs real-time
sudo journalctl -u cvedix-ai-runtime -f

# Xem logs gần đây
sudo journalctl -u cvedix-ai-runtime -n 100
```

## Cấu trúc Package

Sau khi cài đặt:

```
/etc/systemd/system/
  └── cvedix-ai-runtime.service

/etc/cvedix/
  ├── service.conf (tạo từ example sau khi cài đặt)
  └── service.conf.example

/usr/bin/
  └── cvedix-service-wrapper

/opt/cvedix/
  └── (working directory, cần có cvedix_data)

/var/log/cvedix/
  └── (log directory)

/usr/share/doc/cvedix-ai-runtime/
  ├── README.md
  ├── README_SERVICE.md
  └── RELEASE_NOTES.md
```

## User và Permissions

Service chạy dưới user `cvedix` (system user) được tạo tự động khi cài đặt:
- User: `cvedix`
- Group: `cvedix`
- Home: `/opt/cvedix`
- Shell: `/bin/bash`

## Quản lý Service

```bash
# Dừng service
sudo systemctl stop cvedix-ai-runtime

# Khởi động lại
sudo systemctl restart cvedix-ai-runtime

# Vô hiệu hóa tự động khởi động
sudo systemctl disable cvedix-ai-runtime

# Xem trạng thái chi tiết
sudo systemctl status cvedix-ai-runtime
```

## Troubleshooting

### Service không khởi động

1. Kiểm tra logs:
```bash
sudo journalctl -u cvedix-ai-runtime -n 50
```

2. Kiểm tra sample binary:
```bash
ls -la /usr/bin/*_sample
```

3. Kiểm tra cấu hình:
```bash
cat /etc/cvedix/service.conf
```

4. Kiểm tra permissions:
```bash
sudo -u cvedix ls -la /opt/cvedix
```

### Service liên tục restart

Xem logs để tìm nguyên nhân:
```bash
sudo journalctl -u cvedix-ai-runtime -f
```

Có thể do:
- Sample binary lỗi
- Thiếu models hoặc dependencies
- Vấn đề về permissions

## Lưu ý

1. **Samples cần được build**: Để service có thể chạy, cần build với `-DCVEDIX_BUILD_SAMPLES=ON` hoặc sử dụng `--samples` flag khi build package.

2. **cvedix_data directory**: Service cần thư mục `cvedix_data` trong working directory (`/opt/cvedix`). Đảm bảo copy thư mục này trước khi khởi động service.

3. **Configuration**: Service không tự động kích hoạt sau khi cài đặt. Cần cấu hình và khởi động thủ công.

4. **Security**: Service chạy với các security restrictions (NoNewPrivileges, PrivateTmp, ProtectSystem) để tăng tính bảo mật.

## Tài liệu thêm

Xem `debian/README_SERVICE.md` hoặc `/usr/share/doc/cvedix-ai-runtime/README_SERVICE.md` sau khi cài đặt để biết thêm chi tiết.

