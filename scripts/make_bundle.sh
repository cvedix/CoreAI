#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 [build_dir]"
  echo "  build_dir: Path to the build directory"
  exit 1
}

# ===== Parse arguments =====
if [ "$#" -gt 1 ]; then
  usage
fi

BUILD_DIR=""

if [ "$#" -eq 1 ]; then
  BUILD_DIR="$1"
fi

if [ -z "${BUILD_DIR}" ]; then
  usage
fi

BUNDLE_NAME="cvedix-ai-runtime-complete"
VERSION="$(date +%Y.%m.%d)"

# ===== Config =====
BUNDLE_DIR="bundle"

CVEDIX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_SRC="${BUILD_DIR}/bin"
LIB_SRC="${BUILD_DIR}/libs"
CVEDIX_SRC="${BUILD_DIR}/cvedix"

CVEDIX_DATA_SRC="${CVEDIX_ROOT}/cvedix_data"
CVEDIX_SAMPLE_SRC="${CVEDIX_ROOT}/samples"

# ===== Clean & create bundle structure =====
echo "[*] Preparing bundle folder..."
rm -rf "${BUNDLE_DIR}"

mkdir -p \
  "${BUNDLE_DIR}/sample" \
  "${BUNDLE_DIR}/cvedix_data" \
  "${BUNDLE_DIR}/include/cvedix" \
  "${BUNDLE_DIR}/lib/cvedix"

# ===== Copy ELF binaries and cpp code =====
if [ -d "${BIN_SRC}" ]; then
  echo "[*] Copying ELF binaries and corresponding source files..."
  while IFS= read -r -d '' f; do
    if file "$f" | grep -q "ELF"; then
      # Copy the ELF binary
      cp -a "$f" "${BUNDLE_DIR}/sample/"
      
      # Check for corresponding C++ source file
      basename_elf=$(basename "$f")
      cpp_src="${CVEDIX_SAMPLE_SRC}/${basename_elf}.cpp"
      
      if [ -f "$cpp_src" ]; then
        echo "  -> Found source: $cpp_src, copying..."
        cp -a "$cpp_src" "${BUNDLE_DIR}/sample/"
      fi
    fi
  done < <(find "${BIN_SRC}" -maxdepth 1 -type f -print0)
fi

# ===== Copy cvedix_data ====

if [ -d "${CVEDIX_DATA_SRC}" ]; then
  echo "[*] Copying cvedix_data from source..."
  cp -r "${CVEDIX_DATA_SRC}"/* "${BUNDLE_DIR}/cvedix_data/"

  # Create symlink in sample/
  ln -s "../cvedix_data" "${BUNDLE_DIR}/sample/cvedix_data"

fi

# ===== Copy headers (keep folder structure) =====
copy_headers() {
  local SRC="$1"
  local DST="$2"

  if [ ! -d "${SRC}" ]; then
    return
  fi

  echo "[*] Copying headers from ${SRC} -> ${DST}"

  # Copy whole tree structure
    rsync -aL \
    --include='*/' \
    --include='*.h' \
    --include='*.hpp' \
    --exclude='*' \
    "${SRC}/" "${DST}/"
}

copy_headers "${CVEDIX_SRC}" "${BUNDLE_DIR}/include/cvedix"


BUNDLE_LIB="$BUNDLE_DIR/lib"
CVEDIX_LIB="${BUNDLE_LIB}/cvedix"
CUDA_DST="$BUNDLE_LIB/cuda"
CUDNN_DST="$BUNDLE_LIB/cudnn"
TENSORRT_DST="$BUNDLE_LIB/tensorrt"
OPENCV_DST="$BUNDLE_LIB/opencv"

CUDA_SRC_PATH="/usr/local/cuda"           # Adjust if necessary
TENSORRT_SRC_PATH="/usr/local/tensorRT"   # Adjust if necessary
OPENCV_SRC_PATH="/usr/local/lib"          # Adjust if necessary

mkdir -p "$CUDA_DST" "$CUDNN_DST" "$TENSORRT_DST" "$OPENCV_DST"

# ===== Copy shared libraries =====
if [ -d "${LIB_SRC}" ]; then
  echo "[*] Copying shared libraries..."
  find "${LIB_SRC}" -type f -name "*.so*" -exec cp -a {} "${BUNDLE_DIR}/lib/cvedix/" \;
fi
# ===== Bundle 3rd party shared libraries =====
echo "[+] Bundling CUDA runtime"
cp -a $CUDA_SRC_PATH/lib64/libcudart*.so* \
      $CUDA_SRC_PATH/lib64/libcublas*.so* \
      $CUDA_SRC_PATH/lib64/libnvrtc*.so* \
      "$CUDA_DST"/

echo "[+] Bundling cuDNN"
cp -a $CUDA_SRC_PATH/lib64/libcudnn*.so* "$CUDNN_DST"/

echo "[+] Bundling TensorRT"
cp -a \
  $TENSORRT_SRC_PATH/lib/libnvinfer*.so* \
  $TENSORRT_SRC_PATH/lib/libnvinfer_plugin*.so* \
  $TENSORRT_SRC_PATH/lib/libnvonnxparser*.so* \
  "$TENSORRT_DST"/

echo "[+] Bundling OpenCV"
cp -a $OPENCV_SRC_PATH/libopencv*.so* "$OPENCV_DST"/

echo "[✓] Stack-based bundling completed"


# ===== Create README.md =====

cat > "$BUNDLE_DIR/README.md" << 'README_EOF'
# CVEDIX AI Runtime Bundle

## Giới thiệu

Đây là bundle chạy offline của **CVEDIX AI Runtime**, bao gồm:
- Tất cả shared libraries cần thiết
- Các sample executable
- Models (TensorRT / ONNX / Paddle)
- Video test & config

👉 Giải nén là chạy, **không cần cài CUDA / TensorRT / OpenCV**.

---

## Yêu cầu hệ thống

- Linux x86_64
- NVIDIA GPU (compute capability ≥ 6.0)
- NVIDIA Driver phù hợp (đã cài trên host)

---

bundle/
├── sample/            # ELF executables
├── lib/
│   ├── cvedix/
│   ├── cuda/
│   ├── cudnn/
│   ├── tensorrt/
│   ├── opencv/
│   └── paddle/
├── include/        # Header files
├── cvedix_data/     # Models, test videos, test images
├── VERSION
└── README.md

README_EOF

# Create VERSION file
cat > "$BUNDLE_DIR/VERSION" << VERSION_EOF
BUNDLE_NAME=${BUNDLE_NAME}
VERSION=${VERSION}
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")
BUILD_TIMESTAMP=$(date +%s)

ARCHITECTURE=$(uname -m)
PLATFORM=$(uname -s)
KERNEL=$(uname -r)

GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

CUDA_VERSION=$(nvcc --version 2>/dev/null | grep "release" | awk '{print $6}' | tr -d ',' || echo "unknown")
TRT_VERSION=$(cat /usr/local/tensorRT/include/NvInferVersion.h 2>/dev/null | grep TRT_MAJOR_ENTERPRISE | head -1 | awk '{print $3}' || echo "unknown")
OPENCV_VERSION=$(pkg-config --modversion opencv4 2>/dev/null || echo "unknown")

VERSION_EOF

echo "✓ VERSION file created"


# ===== Create .deb package =====
PKG_NAME="cvedix-ai-runtime"
PKG_VERSION="${VERSION}"
ARCH="amd64"   # hoặc arm64
DEB_DIR="deb_pkg"
INSTALL_PREFIX="/opt/${PKG_NAME}"

echo "[*] Preparing deb package layout..."

rm -rf "${DEB_DIR}"
mkdir -p \
  "${DEB_DIR}/DEBIAN" \
  "${DEB_DIR}${INSTALL_PREFIX}"

cp -r "${BUNDLE_DIR}/." "${DEB_DIR}${INSTALL_PREFIX}/"

cat > "${DEB_DIR}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: libs
Priority: optional
Architecture: ${ARCH}
Maintainer: CVEDIX Team <dev@cvedix.ai>
Depends: build-essential, make, cmake, pkg-config, mosquitto, mosquitto-clients, unzip, libmosquitto-dev, libturbojpeg, libturbojpeg-dev, libgstreamer1.0-0, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-ugly, gstreamer1.0-libav, gstreamer1.0-tools, gstreamer1.0-x, gstreamer1.0-alsa, gstreamer1.0-gl, gstreamer1.0-gtk3, gstreamer1.0-qt5, gstreamer1.0-pulseaudio, libgstreamer1.0-dev, libgstreamer-plugins-base1.0-dev, python3-gst-1.0, libgstrtspserver-1.0-dev, gstreamer1.0-rtsp, libgtk-3-dev, libavcodec-dev, libavformat-dev, libavdevice-dev, libavutil-dev, libswscale-dev, libswresample-dev, libv4l-dev, libxvidcore-dev, libx264-dev, libjpeg-dev, libpng-dev, libtiff-dev, gfortran, openexr, libatlas-base-dev, python3-dev, python3-numpy, libeigen3-dev, librdkafka-dev, libssl-dev, libcurl4-openssl-dev, libjson-c-dev
Description: CVEDIX AI Runtime Bundle
 Runtime libraries, headers and sample binaries for CVEDIX AI stack.
EOF


cat > "${DEB_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

CONF_FILE="/etc/ld.so.conf.d/cvedix-ai-runtime.conf"

cat > "$CONF_FILE" <<EOL
/opt/cvedix-ai-runtime/lib/cvedix
/opt/cvedix-ai-runtime/lib/cuda
/opt/cvedix-ai-runtime/lib/cudnn
/opt/cvedix-ai-runtime/lib/tensorrt
/opt/cvedix-ai-runtime/lib/opencv
EOL

ldconfig

exit 0
EOF

chmod 755 "${DEB_DIR}/DEBIAN/postinst"

cat > "${DEB_DIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e

rm -f /etc/ld.so.conf.d/cvedix-ai-runtime.conf
ldconfig

exit 0
EOF

chmod 755 "${DEB_DIR}/DEBIAN/postrm"

dpkg-deb --build "${DEB_DIR}" \
  "${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
