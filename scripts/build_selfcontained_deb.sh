#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

echo -e "${MAGENTA}================================================================${NC}"
echo -e "${MAGENTA}  CVEDIX AI Runtime - Self-Contained Debian Package Builder    ${NC}"
echo -e "${MAGENTA}  (No CUDA/TensorRT installation required on target machine)   ${NC}"
echo -e "${MAGENTA}================================================================${NC}"
echo ""

# Configuration
PACKAGE_NAME="cvedix-ai-runtime"
VERSION="$(date +%Y.%m.%d)"
ARCH="amd64"
INSTALL_PREFIX="/opt/cvedix"
BUILD_DIR="${PWD}/build"
PKG_DIR="${PWD}/deb_package"
CVEDIX_DATA_DIR="${PWD}/cvedix_data"

# Dependency paths
CUDA_LIB_DIR="/usr/local/cuda/lib64"
TRT_LIB_DIR="/usr/lib/x86_64-linux-gnu"
CUDNN_LIB_DIR="/usr/lib/x86_64-linux-gnu"
OPENCV_LIB_DIR="/usr/lib/x86_64-linux-gnu"

echo -e "${BLUE}Configuration:${NC}"
echo -e "  Package Name:    ${YELLOW}$PACKAGE_NAME${NC}"
echo -e "  Version:         ${YELLOW}$VERSION${NC}"
echo -e "  Install Prefix:  ${YELLOW}$INSTALL_PREFIX${NC}"
echo ""

# Check prerequisites
echo -e "${BLUE}[1/12] Checking prerequisites...${NC}"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found.${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/libs/libcvedix_core.so" ]; then
    echo -e "${RED}Error: libcvedix_core.so not found.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Create package directory structure
echo -e "${BLUE}[2/12] Creating package directory structure...${NC}"

rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/lib"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/bin"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/include"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/models"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/test_video"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/config"
mkdir -p "$PKG_DIR/etc/ld.so.conf.d"
mkdir -p "$PKG_DIR/etc/profile.d"

echo -e "${GREEN}✓ Directory structure created${NC}"
echo ""

# Copy CVEDIX libraries
echo -e "${BLUE}[3/12] Copying CVEDIX libraries...${NC}"
cp "$BUILD_DIR/libs/"*.so "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
echo -e "  ${GREEN}✓ CVEDIX libs copied${NC}"

# Copy binaries
echo -e "${BLUE}[4/13] Copying executables...${NC}"
for bin in "$BUILD_DIR/bin/"*; do
    [ -f "$bin" ] && [ -x "$bin" ] && cp "$bin" "$PKG_DIR$INSTALL_PREFIX/bin/" 2>/dev/null || true
done
BIN_COUNT=$(ls -1 "$PKG_DIR$INSTALL_PREFIX/bin/" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $BIN_COUNT executables copied${NC}"

# Copy ALL header files
echo -e "${BLUE}[5/13] Copying header files...${NC}"

# Use output/include if available (complete structured headers)
if [ -d "${PWD}/output/include/cvedix" ]; then
    # Copy entire pre-structured include directory
    cp -r "${PWD}/output/include/cvedix" "$PKG_DIR$INSTALL_PREFIX/include/"
    echo -e "  ${GREEN}✓ Copied from output/include (complete structure)${NC}"
else
    # Fallback: Create include directory structure manually
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/src"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/des"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/osd"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/infers/base"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/transform"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/tracker"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/objects/shapes"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/objects/ba"
    mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/cvedix/utils"

    # Copy node headers
    cp "${PWD}/nodes/src/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/src/" 2>/dev/null || true
    cp "${PWD}/nodes/des/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/des/" 2>/dev/null || true
    cp "${PWD}/nodes/osd/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/osd/" 2>/dev/null || true
    cp "${PWD}/nodes/infers/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/infers/" 2>/dev/null || true
    cp "${PWD}/nodes/infers/base/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/infers/base/" 2>/dev/null || true
    cp "${PWD}/nodes/transform/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/transform/" 2>/dev/null || true
    cp "${PWD}/nodes/tracker/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/tracker/" 2>/dev/null || true
    cp "${PWD}/nodes/cvedix_node.h" "$PKG_DIR$INSTALL_PREFIX/include/cvedix/nodes/" 2>/dev/null || true

    # Copy objects headers including shapes
    cp "${PWD}/objects/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/objects/" 2>/dev/null || true
    cp "${PWD}/objects/shapes/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/objects/shapes/" 2>/dev/null || true
    cp "${PWD}/objects/ba/"*.h "$PKG_DIR$INSTALL_PREFIX/include/cvedix/objects/ba/" 2>/dev/null || true

    # Copy utils headers
    find "${PWD}/utils" -name "*.h" -exec cp {} "$PKG_DIR$INSTALL_PREFIX/include/cvedix/utils/" \; 2>/dev/null || true
fi

# Create third_party directory and copy headers
mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/third_party/trt_yolov11"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/third_party/trt_yolov11_face"
mkdir -p "$PKG_DIR$INSTALL_PREFIX/include/third_party/paddle_ocr"

cp "${PWD}/third_party/trt_yolov11/"*.h "$PKG_DIR$INSTALL_PREFIX/include/third_party/trt_yolov11/" 2>/dev/null || true
cp "${PWD}/third_party/trt_yolov11_face/"*.h "$PKG_DIR$INSTALL_PREFIX/include/third_party/trt_yolov11_face/" 2>/dev/null || true
cp "${PWD}/third_party/paddle_ocr/include/"*.h "$PKG_DIR$INSTALL_PREFIX/include/third_party/paddle_ocr/" 2>/dev/null || true

HEADER_COUNT=$(find "$PKG_DIR$INSTALL_PREFIX/include" -name "*.h" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $HEADER_COUNT header files copied${NC}"

# Copy CUDA libraries
echo -e "${BLUE}[6/13] Copying CUDA libraries...${NC}"
CUDA_LIBS=(
    "libcudart.so*"
    "libcublas.so*"
    "libcublasLt.so*"
    "libnvrtc.so*"
    "libnvrtc-builtins.so*"
    "libcurand.so*"
    "libcusolver.so*"
    "libcusparse.so*"
)
for lib_pattern in "${CUDA_LIBS[@]}"; do
    for lib in $CUDA_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    done
done
echo -e "  ${GREEN}✓ CUDA libs copied${NC}"

# Copy TensorRT libraries
echo -e "${BLUE}[7/13] Copying TensorRT libraries...${NC}"
TRT_LIBS=(
    "libnvinfer.so*"
    "libnvinfer_plugin.so*"
    "libnvinfer_lean.so*"
    "libnvinfer_dispatch.so*"
    "libnvinfer_builder_resource.so*"
    "libnvonnxparser.so*"
    "libnvparsers.so*"
)
for lib_pattern in "${TRT_LIBS[@]}"; do
    for lib in $TRT_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    done
done
echo -e "  ${GREEN}✓ TensorRT libs copied${NC}"

# Copy cuDNN libraries
echo -e "${BLUE}[8/13] Copying cuDNN libraries...${NC}"
CUDNN_LIBS=(
    "libcudnn.so*"
    "libcudnn_ops.so*"
    "libcudnn_cnn.so*"
    "libcudnn_adv.so*"
    "libcudnn_graph.so*"
    "libcudnn_engines*.so*"
    "libcudnn_heuristic.so*"
)
for lib_pattern in "${CUDNN_LIBS[@]}"; do
    for lib in $CUDNN_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    done
done
echo -e "  ${GREEN}✓ cuDNN libs copied${NC}"

# Copy OpenCV libraries
echo -e "${BLUE}[9/13] Copying OpenCV libraries...${NC}"
OPENCV_LIBS=(
    "libopencv_core.so*"
    "libopencv_imgproc.so*"
    "libopencv_imgcodecs.so*"
    "libopencv_highgui.so*"
    "libopencv_videoio.so*"
    "libopencv_dnn.so*"
    "libopencv_calib3d.so*"
    "libopencv_features2d.so*"
    "libopencv_freetype.so*"
    "libopencv_objdetect.so*"
    "libopencv_flann.so*"
)
for lib_pattern in "${OPENCV_LIBS[@]}"; do
    for lib in $OPENCV_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    done
done
echo -e "  ${GREEN}✓ OpenCV libs copied${NC}"

# Copy Paddle Inference libraries
echo -e "${BLUE}[10/13] Copying Paddle Inference libraries...${NC}"
if [ -d "${PWD}/third_party/paddle_inference" ]; then
    cp -a "${PWD}/third_party/paddle_inference/lib/"*.so* "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    cp -a "${PWD}/third_party/paddle_inference/paddle2onnx/lib/"*.so* "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    cp -a "${PWD}/third_party/paddle_inference/mklml/lib/"*.so* "$PKG_DIR$INSTALL_PREFIX/lib/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Paddle libs copied${NC}"
else
    echo -e "  ${YELLOW}⚠ Paddle libs not found${NC}"
fi

# Copy models and data
echo -e "${BLUE}[11/13] Copying models and data...${NC}"
if [ -d "$CVEDIX_DATA_DIR/models" ]; then
    cp -r "$CVEDIX_DATA_DIR/models" "$PKG_DIR$INSTALL_PREFIX/"
    echo -e "  ${GREEN}✓ Models copied${NC}"
fi
if [ -d "$CVEDIX_DATA_DIR/test_video" ]; then
    cp -r "$CVEDIX_DATA_DIR/test_video/"* "$PKG_DIR$INSTALL_PREFIX/test_video/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Test videos copied${NC}"
fi

# Create ldconfig configuration
echo -e "${BLUE}[12/13] Creating system configuration files...${NC}"

cat > "$PKG_DIR/etc/ld.so.conf.d/cvedix.conf" << EOF
# CVEDIX AI Runtime library path
$INSTALL_PREFIX/lib
EOF

# Create environment setup script
cat > "$PKG_DIR/etc/profile.d/cvedix.sh" << 'EOF'
# CVEDIX AI Runtime environment
export CVEDIX_HOME="/opt/cvedix"
export PATH="$CVEDIX_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$CVEDIX_HOME/lib:$LD_LIBRARY_PATH"
EOF

# Create VERSION file
cat > "$PKG_DIR$INSTALL_PREFIX/VERSION" << VERSION_EOF
PACKAGE_NAME=$PACKAGE_NAME
VERSION=$VERSION
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")
BUILD_TIMESTAMP=$(date +%s)
ARCHITECTURE=$ARCH
PLATFORM=Linux
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
VERSION_EOF

echo -e "  ${GREEN}✓ System configurations created${NC}"

# Create DEBIAN control files
echo -e "${BLUE}[13/13] Creating Debian package metadata...${NC}"

# Calculate installed size
INSTALLED_SIZE=$(du -sk "$PKG_DIR" | cut -f1)

cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Section: devel
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_SIZE
Maintainer: CVEDIX Team <support@cvedix.com>
Homepage: https://cvedix.com
Depends: libc6, libstdc++6, libgstreamer1.0-0, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good
Recommends: nvidia-driver-535 | nvidia-driver-545 | nvidia-driver-550
Description: CVEDIX AI Runtime SDK - Self-Contained Package
 Complete AI video processing SDK with bundled CUDA, TensorRT, cuDNN and OpenCV.
 Includes:
  - Face Detection (YOLOv11 TensorRT)
  - License Plate Detection (YOLOv11 TensorRT)
  - License Plate Recognition (PaddleOCR)
  - Object Detection (YOLOv8/v11)
 .
 This package includes all required dependencies and does NOT require
 separate CUDA, TensorRT, or OpenCV installation.
 .
 Requirements: NVIDIA GPU with driver >= 535
EOF

# Create postinst script (runs after installation)
cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

# Update library cache
ldconfig

# Create symbolic links for easy access
ln -sf /opt/cvedix/bin/* /usr/local/bin/ 2>/dev/null || true

# Set permissions
chmod -R 755 /opt/cvedix/bin/
chmod -R 644 /opt/cvedix/lib/*.so* 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  CVEDIX AI Runtime installed successfully!                   ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Installation: /opt/cvedix                                   ║"
echo "║                                                              ║"
echo "║  To use, either:                                             ║"
echo "║  1. Log out and log back in, OR                              ║"
echo "║  2. Run: source /etc/profile.d/cvedix.sh                     ║"
echo "║                                                              ║"
echo "║  Quick test:                                                 ║"
echo "║  $ cd /opt/cvedix                                            ║"
echo "║  $ ./bin/yolov11_face_detector_trt_sample \\                  ║"
echo "║      ./models/trt/face/yolov11_face_fp16.engine \\            ║"
echo "║      ./test_video/face.mp4                                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

# Create postrm script (runs after removal)
cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e

# Remove symbolic links
for bin in /opt/cvedix/bin/*; do
    [ -f "$bin" ] && rm -f "/usr/local/bin/$(basename $bin)" 2>/dev/null || true
done

# Update library cache
ldconfig

echo "CVEDIX AI Runtime removed."
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

# Create conffiles (config files that won't be overwritten on upgrade)
cat > "$PKG_DIR/DEBIAN/conffiles" << EOF
/etc/ld.so.conf.d/cvedix.conf
/etc/profile.d/cvedix.sh
EOF

echo -e "  ${GREEN}✓ Debian metadata created${NC}"
echo ""

# Build package
echo -e "${CYAN}Building Debian package...${NC}"
DEBFILE="${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build "$PKG_DIR" "$DEBFILE"

# Get package size
DEB_SIZE=$(du -sh "$DEBFILE" | cut -f1)

# Summary
echo ""
echo -e "${MAGENTA}================================================================${NC}"
echo -e "${GREEN}Debian package created successfully!${NC}"
echo -e "${MAGENTA}================================================================${NC}"
echo ""
echo -e "Package file: ${YELLOW}$(pwd)/$DEBFILE${NC}"
echo -e "Package size: ${YELLOW}$DEB_SIZE${NC}"
echo ""
echo -e "${BLUE}Library contents:${NC}"
LIB_SIZE=$(du -sh "$PKG_DIR$INSTALL_PREFIX/lib" 2>/dev/null | cut -f1)
echo -e "  Total libs: ${YELLOW}$LIB_SIZE${NC}"
echo ""
echo -e "${CYAN}Installation on target machine:${NC}"
echo -e "  ${YELLOW}sudo dpkg -i $DEBFILE${NC}"
echo -e "  ${YELLOW}sudo apt-get install -f  # Install any missing dependencies${NC}"
echo ""
echo -e "${CYAN}After installation:${NC}"
echo -e "  ${YELLOW}source /etc/profile.d/cvedix.sh${NC}"
echo -e "  ${YELLOW}cd /opt/cvedix${NC}"
echo -e "  ${YELLOW}./bin/yolov11_face_detector_trt_sample ./models/trt/face/yolov11_face_fp16.engine ./test_video/face.mp4${NC}"
echo ""
echo -e "${GREEN}Done!${NC}"
