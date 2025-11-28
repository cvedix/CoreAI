# CVEDIX AI Runtime Service Configuration

## Tổng quan

Package này bao gồm cấu hình systemd service để chạy CVEDIX AI Runtime như một service hệ thống.

## Cài đặt Service

Sau khi cài đặt package, service sẽ được cài đặt nhưng chưa được kích hoạt. Để sử dụng:

### 1. Cấu hình Service

Chỉnh sửa file cấu hình:
```bash
sudo nano /etc/cvedix/service.conf
```

Các tùy chọn cấu hình:
- `CVEDIX_SAMPLE`: Tên sample executable cần chạy (ví dụ: `1-1-1_sample`, `face_tracking_sample`)
- `CVEDIX_WORK_DIR`: Thư mục làm việc (mặc định: `/opt/cvedix`)

Ví dụ:
```bash
CVEDIX_SAMPLE="1-1-1_sample"
CVEDIX_WORK_DIR="/opt/cvedix"
```

### 2. Đảm bảo cvedix_data có sẵn

Service cần thư mục `cvedix_data` trong thư mục làm việc. Đảm bảo:
```bash
ls -la /opt/cvedix/cvedix_data/
```

### 3. Khởi động Service

```bash
# Khởi động service
sudo systemctl start cvedix-ai-runtime

# Kích hoạt service để tự động khởi động khi boot
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

# Xem logs từ một thời điểm cụ thể
sudo journalctl -u cvedix-ai-runtime --since "2025-01-01 00:00:00"
```

## Quản lý Service

### Dừng Service
```bash
sudo systemctl stop cvedix-ai-runtime
```

### Khởi động lại Service
```bash
sudo systemctl restart cvedix-ai-runtime
```

### Vô hiệu hóa Service (không tự động khởi động khi boot)
```bash
sudo systemctl disable cvedix-ai-runtime
```

### Xem trạng thái chi tiết
```bash
sudo systemctl status cvedix-ai-runtime
```

## Cấu trúc Service

- **Service file**: `/etc/systemd/system/cvedix-ai-runtime.service`
- **Wrapper script**: `/usr/bin/cvedix-service-wrapper`
- **Config file**: `/etc/cvedix/service.conf`
- **Config example**: `/etc/cvedix/service.conf.example`
- **Working directory**: `/opt/cvedix`
- **Log directory**: `/var/log/cvedix`

## User và Permissions

Service chạy dưới user `cvedix` (system user) với các quyền hạn chế:
- Working directory: `/opt/cvedix` (read-write)
- Log directory: `/var/log/cvedix` (read-write)
- Config directory: `/etc/cvedix` (read-only)

## Troubleshooting

### Service không khởi động được

1. Kiểm tra logs:
```bash
sudo journalctl -u cvedix-ai-runtime -n 50
```

2. Kiểm tra sample binary có tồn tại:
```bash
ls -la /usr/bin/*_sample
```

3. Kiểm tra cấu hình:
```bash
cat /etc/cvedix/service.conf
```

4. Kiểm tra quyền truy cập:
```bash
sudo -u cvedix ls -la /opt/cvedix
```

### Service tự động restart

Nếu service liên tục restart, kiểm tra:
- Sample binary có lỗi khi chạy
- Thiếu dependencies hoặc models
- Vấn đề về permissions

Xem logs để biết lý do:
```bash
sudo journalctl -u cvedix-ai-runtime -f
```

### Thay đổi sample đang chạy

1. Dừng service:
```bash
sudo systemctl stop cvedix-ai-runtime
```

2. Cập nhật config:
```bash
sudo nano /etc/cvedix/service.conf
# Thay đổi CVEDIX_SAMPLE
```

3. Khởi động lại:
```bash
sudo systemctl start cvedix-ai-runtime
```

## Các Sample có sẵn

Sau khi build với `-DCVEDIX_BUILD_SAMPLES=ON`, các sample sau có thể được sử dụng:

- `1-1-1_sample`: Sample cơ bản 1 input, 1 task, 1 output
- `1-1-N_sample`: 1 input, 1 task, N outputs
- `face_tracking_sample`: Face detection và tracking
- `rknn_face_detection_sample`: Face detection với RKNN
- `rtsp_src_sample`: Đọc video từ RTSP stream
- Và nhiều sample khác...

Xem danh sách đầy đủ:
```bash
ls -1 /usr/bin/*_sample
```

## Ghi chú

- Service sẽ tự động restart nếu bị crash (RestartSec=10)
- Logs được ghi vào systemd journal
- Service chạy với security restrictions để tăng tính bảo mật
- Cần đảm bảo `cvedix_data` directory có sẵn trong working directory

