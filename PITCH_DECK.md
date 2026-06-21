# CVEDIX AI Runtime SDK
## Pitch Deck — Product Sales

---

## 1. Vấn đề thị trường

Các doanh nghiệp đang triển khai AI vision tại Việt Nam đối mặt với những thách thức:

| Thách thức | Mức độ nghiêm trọng |
|------------|---------------------|
| **Fragmented hardware** | Cần hỗ trợ nhiều nền tảng: NVIDIA GPU, Intel CPU, Rockchip NPU |
| **Vendor lock-in** | Mỗi backend (TensorRT, OpenVINO, ONNX) yêu cầu code riêng |
| **Slow time-to-market** | Tốn 6-12 tháng để build pipeline từ scratch |
| **Complex deployment** | Thiếu packaging, monitoring, và management tools |
| **Real-time requirement** | Cần xử lý video multi-channel với latency < 50ms |
| **Limited flexibility** | Không thể hot-swap model hoặc add new analytics without restart |

---

## 2. Giải pháp — CVEDIX AI Runtime SDK

**"One SDK. All Backends. Edge-Ready."**

CVEDIX AI Runtime SDK là framework C++ plugin-based cho phép doanh nghiệp:
- **Build một lần, chạy mọi nền tảng** (x86, ARM, GPU, NPU)
- **Swappable inference backends** (TensorRT ↔ OpenVINO ↔ ONNX ↔ RKNN)
- **Hot-plug analytics nodes** — thêm/bỏ tính năng không cần restart
- **Production-ready** — .deb package, systemd service, real-time monitoring

---

## 3. Tính năng cốt lõi

### 🎯 Đa Backend Inference
```
Cùng một YOLO model → Chạy trên TensorRT (NVIDIA GPU) / OpenVINO (Intel) / RKNN (Rockchip NPU)
```
- **ONNX Runtime**: CPU inference mặc định
- **TensorRT**: GPU acceleration (3-10x faster)
- **OpenVINO**: Intel CPU optimization
- **RKNN**: Rockchip NPU (RK3588)
- **PaddlePaddle**: OCR & detection (Chinese models)

### 🧩 Plugin Architecture — 80+ Nodes
| Category | Tính năng |
|----------|-----------|
| **Source** | RTSP, RTMP, UDP, File, Image |
| **Detection** | YOLOv8/v11/v26, SSD, RetinaFace |
| **Recognition** | Face Recognition, License Plate, OCR |
| **Tracking** | ByteTrack, DeepSORT, BoTSORT |
| **Behavior Analysis** | 22+ rules: crossing, loitering, speed, crowding |
| **Output** | MQTT, Kafka, RTSP, File, Screen |

### ⚡ Real-Time Performance
- Multi-channel video processing (1-32 channels)
- Sub-50ms latency per frame
- GPU-accelerated inference pipeline
- Built-in analysis board (FPS, latency, queue stats)

### 🌐 Edge-First Design
- Deploy trên: NVIDIA Jetson, Rockchip RK3588, Intel NUC
- Deb package vào `/opt/cvedix` — không conflict hệ thống
- systemd service với auto-restart
- Zero-downtime updates

---

## 4. Kiến trúc kỹ thuật

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│    Custom pipelines, 60+ sample programs, integration        │
├─────────────────────────────────────────────────────────────┤
│                    SDK Core (.so)                            │
│              Dynamic Node Pipeline Engine                    │
├──────────┬──────────┬──────────┬──────────┬────────────────┤
│ Source   │ Detect   │ Analyze  │ Output   │ Broker         │
│ Nodes    │ Nodes    │ Nodes    │ Nodes    │ Nodes          │
├──────────┴──────────┴──────────┴──────────┴────────────────┤
│         Utils: Logger, AnalysisBoard, FrameMetadata        │
├─────────────────────────────────────────────────────────────┤
│    Inference: ONNX · TensorRT · OpenVINO · RKNN · Paddle   │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. Use Cases — Khách hàng có thể làm gì?

### 🏢 Smart Building
```
RTSP Camera → Face Detection → Face Recognition → Access Control + Alert
```
- Nhận diện người ra/vào realtime
- Blacklist/whitelist alert qua MQTT
- Lưu trữ sự kiện vào database

### 🛣️ Smart Traffic
```
Multiple RTSP → YOLO Detector → ByteTrack → Behavior Analysis → Traffic Report
```
- Đếm xe, phân loại phương tiện
- Phát hiện vi phạm: vượt đèn đỏ, wrong-way, quá tốc độ
- Xuất báo cáo qua Kafka/MQTT

### 🏪 Smart Retail
```
Cameras → Face Analysis → Queue Detection → Heatmap → Dashboard
```
- Phân tích客流: đông đúc, chờ đợi
- Heatmap theo dõi khu vực nóng
- Behavior analysis: loitering, area intrusion

### 🏭 Smart Factory
```
Cameras → Object Detection → Safety Compliance → Alert System
```
- Phát hiện người không cấp phép
- Monitoring: không đeo helmet, vào khu vực cấm
- Real-time alert qua Webhook/MQTT

---

## 6. So sánh với Alternatives

| Tính năng | **CVEDIX SDK** | **Custom Build** | **Commercial VMS** |
|-----------|----------------|------------------|-------------------|
| Multi-backend | ✅ 7 backends | ❌ 1-2 backends | ⚠️ 1-2 backends |
| Plugin nodes | ✅ 80+ nodes | ❌ Custom toàn bộ | ⚠️ Proprietary |
| Hot-plug | ✅ Không restart | ❌ Cần restart | ❌ Cần restart |
| Edge deployment | ✅ .deb + systemd | ❌ Manual | ❌ Cloud-heavy |
| Platform support | ✅ x86 + ARM + NPU | ⚠️ Tùy chọn | ❌ x86 only |
| Time-to-market | ✅ 1-2 tuần | ❌ 6-12 tháng | ✅ 2-4 tuần |
| License | ✅ Per-project | ✅ Free (dev cost) | ❌ Expensive |

---

## 7. Pricing Model (Gợi ý)

### License Perpetual
| Tier | Price | Features |
|------|-------|----------|
| **Starter** | Liên hệ | 4 channels, CPU only, standard nodes |
| **Professional** | Liên hệ | 16 channels, GPU support, all nodes |
| **Enterprise** | Liên hệ | Unlimited channels, all backends, priority support |

### Subscription (Annual)
| Tier | Price | Features |
|------|-------|----------|
| **Starter** | Liên hệ | 4 channels, updates + support |
| **Professional** | Liên hệ | 16 channels, GPU + updates + support |
| **Enterprise** | Liên hệ | Unlimited, all backends + SLA support |

### Custom Development
- Professional services: design, develop, deploy custom pipelines
- Training workshops cho team kỹ thuật
- Dedicated support contracts

---

## 8. Roadmap Sản Phẩm

| Quarter | Milestone |
|---------|-----------|
| **Q3 2025** | Release v2025.0.1 — Core SDK + 80+ nodes + 7 backends |
| **Q4 2025** | Web dashboard, REST API, docker-compose deployment |
| **Q1 2026** | Cloud-edge sync, remote pipeline management |
| **Q2 2026** | AI model training pipeline, automated node generation |
| **Q3 2026** | Multi-tenant SaaS support, white-label customization |

---

## 9. Lợi ích kinh tế

### Đối với khách hàng
| Lợi ích | Tác động |
|---------|----------|
| **Giảm 80% development time** | Từ 6 tháng → 1-2 tuần |
| **Giảm 60% infrastructure cost** | Chạy trên edge device, không cần cloud |
| **Giảm 50% maintenance** | Plugin architecture, hot updates |
| **ROI trong 3-6 tháng** | Giảm chi phí nhân sự + hardware |

### Đối với đối tác
- **White-label**: Custom branding, custom nodes
- **Revenue share**: Reseller program
- **Technical support**: Joint deployment với customer

---

## 10. Call to Action

### Bắt đầu với CVEDIX AI Runtime SDK

1. **Demo** — Schedule a 30-minute product demo
2. **POC** — 2-week proof of concept với use case của bạn
3. **License** — Chọn tier phù hợp, receive source code + documentation
4. **Deploy** — Team hỗ trợ setup trong 1-2 tuần

### Liên hệ
- **Email**: sales@cvedix.com
- **Website**: [待 thêm URL]
- **Phone**: [待 thêm số điện thoại]

---

> **CVEDIX AI Runtime SDK** — *Build Once. Deploy Everywhere.*
>
> *Proprietary software by CVEDIX. All rights reserved.*