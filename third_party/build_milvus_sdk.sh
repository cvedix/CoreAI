#!/bin/bash
# ============================================================================
# Build Milvus C++ SDK v2.6.3 for CVEDIX Integration
# ============================================================================
# This script clones, builds, and installs milvus-sdk-cpp into a local
# build/ directory that CMakeLists.txt auto-detects.
#
# Prerequisites:
#   - CMake >= 3.14
#   - C++ compiler with C++14+ support (GCC 7+)
#   - Python 3 with pip (for Conan)
#   - Git
#
# Usage:
#   cd third_party/milvus_sdk
#   bash build_milvus_sdk.sh
#
# Output:
#   build/lib/libmilvus_sdk.so
#   build/include/milvus/
# ============================================================================

set -e

# Ensure ~/.local/bin is in PATH for pip-installed binaries like conan
export PATH="$HOME/.local/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_VERSION="v2.6.3"
SDK_REPO="https://github.com/milvus-io/milvus-sdk-cpp.git"
SOURCE_DIR="${SCRIPT_DIR}/milvus-sdk-cpp"
BUILD_DIR="${SCRIPT_DIR}/milvus-sdk-cpp-build"
INSTALL_DIR="${BUILD_DIR}"

echo "============================================"
echo " Building Milvus C++ SDK ${SDK_VERSION}"
echo "============================================"
echo " Source:  ${SOURCE_DIR}"
echo " Install: ${INSTALL_DIR}"
echo "============================================"

# ── Step 1: Initialize Submodule ──
echo "[1/5] Initializing milvus-sdk-cpp submodule..."
cd "${SCRIPT_DIR}/.." # Go to repo root (/home/cvedix/CVEDIX-AI/core)
git submodule update --init --recursive third_party/milvus-sdk-cpp
cd "${SOURCE_DIR}"

# ── Step 2: Install dependencies ──
echo "[2/5] Installing dependencies (Conan, etc.)..."

if ! command -v conan &> /dev/null; then
    echo "Conan not found! Installing conan via pip..."
    pip3 install conan==1.64.1 --break-system-packages || pip install conan==1.64.1 --break-system-packages
fi

if [ -f "scripts/install_deps.sh" ]; then
    bash scripts/install_deps.sh
else
    # Detect conan profile
    conan profile detect --force 2>/dev/null || true
    conan profile update settings.compiler.libcxx=libstdc++11 default 2>/dev/null || true
fi

# ── Step 3: Build with Conan ──
echo "[3/5] Building with Conan + CMake..."
if [ -f "scripts/build.sh" ]; then
    # Use official build script
    bash scripts/build.sh
else
    # Manual build
    mkdir -p cmake_build && cd cmake_build
    
    conan install .. --build=missing -s build_type=Release 2>/dev/null || true
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF
    
    cmake --build . --config Release -j$(nproc)
    cd ..
fi

# ── Step 4: Install to build/ ──
echo "[4/5] Installing to ${INSTALL_DIR}..."
mkdir -p "${INSTALL_DIR}/lib"
mkdir -p "${INSTALL_DIR}/include"

# Find and copy built libraries
find "${SOURCE_DIR}" -name "libmilvus_sdk*.so*" -o -name "libmilvus_sdk*.a" 2>/dev/null | head -5 | while read lib; do
    cp -av "$lib" "${INSTALL_DIR}/lib/" 2>/dev/null || true
done

# Also check cmake_build/lib or build/lib
for search_dir in "${SOURCE_DIR}/cmake_build" "${SOURCE_DIR}/build" "${SOURCE_DIR}/build/Release"; do
    if [ -d "${search_dir}" ]; then
        find "${search_dir}" -name "libmilvus_sdk*.so*" -o -name "libmilvus_sdk*.a" 2>/dev/null | while read lib; do
            cp -av "$lib" "${INSTALL_DIR}/lib/" 2>/dev/null || true
        done
    fi
done

# Copy headers
if [ -d "${SOURCE_DIR}/src/include/milvus" ]; then
    cp -r "${SOURCE_DIR}/src/include/milvus" "${INSTALL_DIR}/include/"
elif [ -d "${SOURCE_DIR}/include/milvus" ]; then
    cp -r "${SOURCE_DIR}/include/milvus" "${INSTALL_DIR}/include/"
fi

# ── Step 5: Verify ──
echo "[5/5] Verifying installation..."
echo ""

LIB_COUNT=$(find "${INSTALL_DIR}/lib" -name "libmilvus*" 2>/dev/null | wc -l)
HDR_COUNT=$(find "${INSTALL_DIR}/include" -name "*.h" -o -name "*.hpp" 2>/dev/null | wc -l)

echo "============================================"
echo " Build Complete!"
echo "============================================"
echo " Libraries: ${LIB_COUNT} files in ${INSTALL_DIR}/lib/"
echo " Headers:   ${HDR_COUNT} files in ${INSTALL_DIR}/include/"
echo ""

if [ "$LIB_COUNT" -gt 0 ] && [ "$HDR_COUNT" -gt 0 ]; then
    echo " ✓ Milvus SDK ready for CVEDIX integration"
    echo ""
    echo " To enable in CVEDIX build:"
    echo "   cmake -DCVEDIX_WITH_MILVUS=ON .."
    echo ""
    ls -la "${INSTALL_DIR}/lib/"
else
    echo " ✗ Build may have issues. Check logs above."
    echo "   Libraries found: ${LIB_COUNT}"
    echo "   Headers found:   ${HDR_COUNT}"
    exit 1
fi
