# Hướng Dẫn Chạy CI trên GPU Self-Hosted Runner

> **Mục tiêu:** Thiết lập và chạy CI/CD pipeline với GPU NVIDIA cho TensorRT/CUDA tests.

---

## Mục lục

1. [Tổng quan](#1-tổng-quan)
2. [Các lựa chọn chạy GPU CI](#2-các-lựa-chọn-chạy-gpu-ci)
3. [Self-Hosted Runner Setup](#3-self-hosted-runner-setup)
4. [GitHub Actions GPU Runner Docker Image](#4-github-actions-gpu-runner-docker-image)
5. [GitHub Actions Enterprise Runner (Actuated)](#5-github-actions-enterprise-runner-actuated)
6. [GitHub Actions GPU Cloud Runners](#6-github-actions-gpu-cloud-runners)
7. [Security & Best Practices](#7-security--best-practices)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Tổng quan

### Tại sao cần Self-Hosted Runner cho GPU CI?

GitHub Actions **standard runners** (ubuntu-latest, ubuntu-22.04, ubuntu-24.04) **KHÔNG có NVIDIA GPU**. Chúng là CPU-only machines chạy trong GitHub's cloud infrastructure.

Để chạy TensorRT, CUDA, hoặc bất kỳ GPU workload nào, bạn cần:

| Phương pháp | GPU | Chi phí | Độ phức tạp | Khuyến nghị |
|-------------|-----|---------|-------------|-------------|
| Self-hosted (bare metal) | ✅ Của bạn | $0 (hardware có sẵn) | Trung bình | ✅✅✅ Tốt nhất |
| Self-hosted (Docker) | ✅ Của bạn | $0 (hardware có sẵn) | Cao | ✅✅ Tốt |
| GitHub Actions Enterprise Runner (Actuated) | ✅ Cloud GPU | $0.3-0.5/giờ | Thấp | ✅✅ Dễ setup |
| GPU Cloud Runner (RunPod, Lambda) | ✅ Cloud | $0.2-0.4/giờ | Thấp | ✅ Phù hợp team nhỏ |

---

## 2. Các lựa chọn chạy GPU CI

### Lựa chọn 1: Self-Hosted Runner (Hardware của bạn) ⭐ KHUYẾN NGHỊ

**Phù hợp khi:** Bạn có server/gpu machine riêng

**Ưu điểm:**
- Miễn phí sau khi có hardware
- Full control
- Chạy được cả local tests

**Nhược điểm:**
- Cần mua và duy trì hardware
- Runner luôn online (tiền điện)

**Hardware requirement tối thiểu:**
- NVIDIA GPU với >= 8GB VRAM (RTX 3060, T4, A10)
- 16GB RAM (32GB khuyến nghị)
- 100GB SSD storage
- Linux (Ubuntu 22.04+)

---

### Lựa chọn 2: GitHub Actions Enterprise Runner (Actuated)

**Phù hợp khi:** Không muốn quản lý hardware

**Ưu điểm:**
- Auto-scale, chỉ chạy khi cần
- Pay-per-use (chỉ trả tiền khi chạy CI)
- GitHub quản lý infrastructure

**Chi phí:** ~$0.3-0.5/giờ cho GPU instance

**Setup:** https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/with-github-hosted-runners-for-enterprises

---

### Lựa chọn 3: GPU Cloud Runner (RunPod, Lambda Labs)

**Phù hợp khi:** Muốn GPU mạnh với chi phí thấp

**Ưu điểm:**
- GPU mạnh hơn (A100, A10)
- Pay-per-hour

**Nhược điểm:**
- Cần setup runner từ cloud machine
- Network latency có thể ảnh hưởng build speed

---

## 3. Self-Hosted Runner Setup

### Bước 1: Chuẩn bị Hardware

```bash
# Kiểm tra GPU
nvidia-smi

# Expected output:
# +-----------------------------------------------------------------------------+
# | NVIDIA-SMI 535.129.03   Driver Version: 535.129.03   CUDA Version: 12.2   |
# |-------------------------------+----------------------+----------------------+
# | GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
# | Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
# |===============================+======================+======================|
# |   0  NVIDIA RTX 3060     Off  | 00000000:01:00.0  On |                  N/A |
# |  45C   P2    25W / 170W |    4095MiB / 12288MiB      5%      Default |
# +-----------------------------------------------------------------------------+
```

Nếu `nvidia-smi` không hoạt động:

```bash
# Cài NVIDIA drivers
sudo apt-get update
sudo apt-get install -y software-properties-common
sudo add-apt-repository -y ppa:graphics-drivers/ppa
sudo apt-get update
sudo apt-get install -y nvidia-driver-535
sudo reboot

# Verify
nvidia-smi
```

---

### Bước 2: Cài Docker + NVIDIA Container Toolkit

```bash
# Cài Docker
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER

# Khởi lại terminal hoặc chạy:
newgrp docker

# Cài NVIDIA Container Toolkit
curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg

curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
  sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list

sudo apt-get update
sudo apt-get install -y nvidia-container-toolkit

# Configure Docker runtime
sudo nvidia-ctk runtime configure --runtime=docker
sudo systemctl restart docker

# Test GPU trong Docker
docker run --rm --gpus all nvidia/cuda:12.2.0-base-ubuntu22.04 nvidia-smi
```

---

### Bước 3: Cài GitHub Actions Runner

```bash
# Tạo directory cho runner
sudo mkdir -p /opt/github-runner
cd /opt/github-runner

# Download runner package
RUNNER_VERSION="2.316.0"  # Use latest version
curl -L -o actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz \
  "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"

tar xzf actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz

# Tạo personal access token hoặc org runner token
# Settings: https://github.com/your-org/your-repo/settings/actions
# Hoặc: https://github.com/organizations/your-org/settings/actions

# Configure runner
./config.sh \
  --url https://github.com/cvedix/rapidmedia \
  --token <YOUR_RUNNER_TOKEN> \
  --labels gpu,self-hosted \
  --work _work \
  --unattended

# Run as systemd service
sudo ./svc.sh install
sudo systemctl enable github-runner
sudo systemctl start github-runner

# Check status
sudo systemctl status github-runner
```

**Alternative: Chạy runner dưới Docker (khuyến nghị cho isolated environment)**

```bash
# Sử dụng GitHub Actions Docker runner
docker run -d \
  --name github-runner \
  --restart unless-stopped \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -e GITHUB_TOKEN=<YOUR_RUNNER_TOKEN> \
  -e RUNNER_NAME=cvedix-gpu-01 \
  -e LABELS=gpu,self-hosted \
  ghcr.io/actions/runner-container:v2.316.0
```

---

### Bước 4: Xác nhận Runner hoạt động

```bash
# Check runner logs
sudo journalctl -u github-runner -f

# Hoặc Docker
docker logs github-runner

# Trên GitHub:
# Settings → Actions → Runners
# Bạn sẽ thấy runner "cvedix-gpu-01" với label "gpu"
```

---

## 4. GitHub Actions GPU Runner Docker Image

### tùy chọn 1: Sử dụng Docker trong runner

```dockerfile
# File: dockerfile-gpu
# Build: docker build -t cvedix-gpu-runner -f dockerfile-gpu .

FROM nvidia/cuda:12.2.0-devel-ubuntu22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libopencv-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libnlohmannjson-dev \
    libspdlog-dev \
    libgtest-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install TensorRT
RUN apt-get update && apt-get install -y \
    libnvinfer8 \
    libnvinfer-plugin8 \
    libnvonnxparsers8 \
    && rm -rf /var/lib/apt/lists/*

# Working directory
WORKDIR /workspace

# Default command
CMD ["bash"]
```

### tùy chọn 2: Custom Docker Image với đầy đủ dependencies

```dockerfile
# File: dockerfile-gpu-full
# Build: docker build -t cvedix/gpu-runner:latest -f dockerfile-gpu-full .

FROM nvidia/cuda:12.2.0-devel-ubuntu22.04

# Environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV CUDA_VERSION=12.2.0
ENV TENSORRT_VERSION=8.6

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    unzip \
    ninja-build \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install OpenCV
RUN apt-get update && apt-get install -y \
    libopencv-dev=4.* \
    && rm -rf /var/lib/apt/lists/*

# Install GStreamer
RUN apt-get update && apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-tools \
    && rm -rf /var/lib/apt/lists/*

# Install TensorRT
RUN curl -L -o tensorrt.deb https://developer.nvidia.com/downloads/compute/machine-learning/tensorrt/secure/8.6.1/tars/tensorrt-8.6.1.6.cuda12.2.deb \
    && apt-get install -y ./tensorrt.deb \
    && rm tensorrt.deb \
    && rm -rf /var/lib/apt/lists/*

# Install JSON and logging libraries
RUN apt-get update && apt-get install -y \
    libnlohmannjson-dev \
    libspdlog-dev \
    libgtest-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install pybind11
RUN git clone --branch v2.11.1 https://github.com/pybind/pybind11.git /opt/pybind11 \
    && cd /opt/pybind11 && python3 tools/setup_build.py install

# Setup CUDA environment
ENV PATH=/usr/local/cuda/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

# Working directory
WORKDIR /workspace

# Verify installation
RUN nvidia-smi && \
    python3 -c "import tensorrt; print('TensorRT version:', tensorrt.__version__)" && \
    python3 -c "import torch; print('PyTorch CUDA:', torch.cuda.is_available())"
```

---

## 5. GitHub Actions Enterprise Runner (Actuated)

### Thiết lập GitHub-hosted GPU runners cho Enterprise

**Bước 1: Đăng ký GitHub Enterprise Cloud**
- Cần GitHub Enterprise Cloud (Business or Enterprise plan)
- Bật tính năng "Hosted runners for enterprise"

**Bước 2: Cấu hình GPU runner set**

```yaml
# .github/workflows/ci-gpu-enterprise.yml
name: CVEDIX GPU CI (Enterprise)

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  gpu-build:
    # Sử dụng GitHub-hosted GPU runner
    runs-on: [self-hosted, gpu, enterprise]
    steps:
      - uses: actions/checkout@v4

      - name: Setup CUDA
        uses: lazytwo/setup-cuda@v1
        with:
          cuda-version: '12.2'

      - name: Build with TensorRT
        run: |
          cmake -S . -B build -DCVEDIX_BACKEND=tensorrt
          cmake --build build -j$(nproc)

      - name: Run GPU tests
        run: |
          cd build
          ctest -R gpu --output-on-failure
```

**Bước 3: Configure auto-scale**

```json
// .github/runner-scale-config.json
{
  "autoScale": {
    "enabled": true,
    "minRunners": 0,
    "maxRunners": 2,
    "scaleDownAfterMinutes": 10
  }
}
```

---

## 6. GitHub Actions GPU Cloud Runners

### tùy chọn 1: Sử dụng RunPod + GitHub Actions

**Bước 1: Tạo GPU instance trên RunPod**

```bash
# Cài GitHub Runner trên RunPod instance
# SSH vào RunPod instance
ssh root@your-runpod-ip

# Cài runner (như Bước 3 ở trên)
cd /opt
curl -L -o runner.tar.gz https://github.com/actions/runner/releases/latest/download/actions-runner-linux-x64.tar.gz
tar xzf runner.tar.gz
./config.sh --url https://github.com/cvedix/rapidmedia --token <TOKEN> --labels gpu,runpod
./svc.sh install
systemctl start github-runner
```

**Bước 2: Trigger CI từ RunPod GPU**

```yaml
# .github/workflows/ci-gpu-runpod.yml
name: CVEDIX GPU CI (RunPod)

on:
  workflow_dispatch:  # Manual trigger
  schedule:
    - cron: '0 2 * * *'  # Daily at 2 AM

jobs:
  gpu-build:
    runs-on: [self-hosted, gpu, runpod]
    steps:
      - uses: actions/checkout@v4
      - name: Build and test GPU
        run: |
          docker build --target gpu-runtime -t cvedix/gpu-test .
          docker run --gpus all cvedix/gpu-test ctest --output-on-failure
```

### tùy chọn 2: Lambda Labs GPU Instances

```bash
# SSH vào Lambda instance
ssh -i ~/.ssh/lambda.pem ubuntu@your-lambda-ip

# Cài Docker
curl -fsSL https://get.docker.com | sh

# Cài NVIDIA drivers (Lambda đã có sẵn drivers)
nvidia-smi  # Verify

# Cài GitHub Runner
cd /opt
curl -L -o runner.tar.gz https://github.com/actions/runner/releases/latest/download/actions-runner-linux-x64.tar.gz
tar xzf runner.tar.gz
./config.sh --url https://github.com/cvedix/rapidmedia --token <TOKEN> --labels gpu,lambda
./svc.sh install
systemctl start github-runner
```

---

## 7. Security & Best Practices

### 7.1 Runner Security

```bash
# ✅ CHẠY runner dưới non-root user
sudo useradd -m github-runner
sudo su - github-runner
cd /opt/github-runner
./config.sh --token <TOKEN> ...

# ❌ KHÔNG chạy runner với sudo/root
```

### 7.2 Token Security

```bash
# ✅ Tạo dedicated runner token
# Settings → Actions → Runners → New runner → Generate token

# ❌ KHÔNG commit token vào git
echo "<RUNNER_TOKEN>" > /tmp/token.txt
# Sau khi config, xóa token
rm /tmp/token.txt
```

### 7.3 GPU Resource Management

```yaml
# Giới hạn GPU usage trong CI
jobs:
  gpu-build:
    runs-on: [self-hosted, gpu]
    steps:
      - name: Build with limited GPU mem
        run: |
          # Limit GPU memory usage
          export CUDA_MEMORY_FRACTION=0.8
          docker run --rm --gpus all \
            -e CUDA_MEMORY_FRACTION=0.8 \
            cvedix/gpu-test ./build
```

### 7.4 Runner Auto-Restart

```bash
# systemd auto-restart configuration
sudo systemctl edit github-runner
# Add:
# [Service]
# Restart=always
# RestartSec=10

sudo systemctl daemon-reload
sudo systemctl restart github-runner
```

---

## 8. Troubleshooting

### Vấn đề 1: Runner không hiện trong GitHub

```bash
# Check runner logs
sudo journalctl -u github-runner -n 100

# Kiểm tra network
curl -I https://github.com
curl -I https://api.github.com

# Kiểm tra token
# Token hết hạn sau 1 giờ, generate lại nếu cần
```

### Vấn đề 2: Docker không thấy GPU

```bash
# Kiểm tra NVIDIA Container Toolkit
nvidia-ctk runtime configure --runtime=docker
sudo systemctl restart docker

# Test
docker run --rm --gpus all nvidia/cuda:12.2.0-base-ubuntu22.04 nvidia-smi

# Nếu lỗi "NVIDIA GPU not found":
sudo nvidia-ctk configure --set=nvidia-container-cli.no-cgroups=true
sudo systemctl restart docker
```

### Vấn đề 3: Build fail do thiếu CUDA libraries

```bash
# Thêm CUDA paths
echo '/usr/local/cuda/lib64' | sudo tee /etc/ld.so.conf.d/cuda.conf
sudo ldconfig

# Verify
ldconfig -p | grep cuda
```

### Vấn đề 4: TensorRT không load

```bash
# Kiểm tra TensorRT installation
ldconfig -p | grep nvinfer

# Nếu không thấy, reinstall:
sudo apt-get install -y libnvinfer8 libnvinfer-plugin8
```

---

## Appendix: Full Setup Script

```bash
#!/bin/bash
# File: setup-gpu-runner.sh
# Usage: sudo bash setup-gpu-runner.sh <GITHUB_TOKEN>

set -e

GITHUB_TOKEN=$1
REPO="cvedix/rapidmedia"
RUNNER_NAME="cvedix-gpu-01"

echo "=== Step 1: Install NVIDIA Drivers ==="
nvidia-smi || {
    echo "Installing NVIDIA drivers..."
    sudo apt-get update
    sudo apt-get install -y software-properties-common
    sudo add-apt-repository -y ppa:graphics-drivers/ppa
    sudo apt-get update
    sudo apt-get install -y nvidia-driver-535
    sudo reboot
}

echo "=== Step 2: Install Docker ==="
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER

echo "=== Step 3: Install NVIDIA Container Toolkit ==="
curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
  sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
sudo apt-get update
sudo apt-get install -y nvidia-container-toolkit
sudo nvidia-ctk runtime configure --runtime=docker
sudo systemctl restart docker

echo "=== Step 4: Install GitHub Actions Runner ==="
RUNNER_VERSION="2.316.0"
sudo mkdir -p /opt/github-runner
sudo chown $USER /opt/github-runner
cd /opt/github-runner

curl -L -o runner.tar.gz \
  "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"
tar xzf runner.tar.gz

./config.sh \
  --url https://github.com/$REPO \
  --token $GITHUB_TOKEN \
  --labels gpu,self-hosted \
  --work _work

sudo ./svc.sh install
sudo systemctl enable github-runner
sudo systemctl start github-runner

echo "=== Step 5: Verify ==="
sudo systemctl status github-runner --no-pager

echo ""
echo "✅ GPU Runner setup complete!"
echo "Runner name: $RUNNER_NAME"
echo "Labels: gpu, self-hosted"
echo "Check at: https://github.com/$REPO/settings/actions"
```

---

## Summary: Nên chọn phương pháp nào?

| Tình huống | Phương pháp | Lý do |
|------------|-------------|-------|
| Có server GPU riêng | Self-hosted (bare metal) | Miễn phí, full control |
| Team nhỏ, không có GPU | Lambda Labs / RunPod | Pay-per-hour, không đầu tư hardware |
| Enterprise | GitHub Enterprise Runner | Auto-scale, GitHub quản lý |
| Test trước khi đầu tư | GitHub Actions + Actuated | Setup nhanh, thử nghiệm |

**Khuyến nghị cho CVEDIX:**

1. **Giai đoạn đầu:** Sử dụng GitHub-hosted GPU runners (Actuated) để test pipeline
2. **Giai đoạn production:** Setup self-hosted runner với hardware có sẵn hoặc thuê cloud GPU