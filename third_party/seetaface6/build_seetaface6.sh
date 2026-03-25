#!/bin/bash
# build_seetaface6.sh - Build SeetaFace6 libraries from source
# Usage: bash build_seetaface6.sh [--clean]
#
# Builds in order: OpenRoleZoo → SeetaAuthorize → TenniS → SDK modules
# Output: build/lib/*.so and build/include/seeta/*.h

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/build"
INSTALL_DIR="${BUILD_ROOT}"
NPROC=$(nproc 2>/dev/null || echo 4)

# ── GPU/CUDA configuration ──
# Usage: bash build_seetaface6.sh [--clean] [--gpu]
TS_USE_CUDA=OFF
CUDA_EXTRA_FLAGS=""
CUDA_ROOT_FLAG=""

if [ "$1" = "--gpu" ] || [ "$2" = "--gpu" ]; then
    # Find best CUDA toolkit (prefer /usr/local/cuda-* over system nvcc)
    BEST_NVCC=""
    BEST_CUDA_DIR=""
    for cuda_dir in $(ls -d /usr/local/cuda-* 2>/dev/null | sort -V -r); do
        if [ -x "${cuda_dir}/bin/nvcc" ]; then
            BEST_NVCC="${cuda_dir}/bin/nvcc"
            BEST_CUDA_DIR="${cuda_dir}"
            break
        fi
    done

    # Fallback to system nvcc
    if [ -z "$BEST_NVCC" ] && command -v nvcc &>/dev/null; then
        BEST_NVCC=$(which nvcc)
    fi

    if [ -n "$BEST_NVCC" ]; then
        CUDA_VER=$($BEST_NVCC --version 2>/dev/null | grep "release" | sed 's/.*release //' | sed 's/,.*//')
        GCC_VER=$(gcc -dumpversion 2>/dev/null | cut -d. -f1)
        CUDA_MAJOR=$(echo "$CUDA_VER" | cut -d. -f1)
        CUDA_MINOR=$(echo "$CUDA_VER" | cut -d. -f2)

        echo "[GPU] Found CUDA ${CUDA_VER} at ${BEST_NVCC}, GCC ${GCC_VER}"

        # CUDA 12.2+ supports GCC 13
        if [ "$GCC_VER" -ge 13 ] && ([ "$CUDA_MAJOR" -lt 12 ] || ([ "$CUDA_MAJOR" -eq 12 ] && [ "$CUDA_MINOR" -lt 2 ])); then
            echo "[GPU] WARNING: CUDA ${CUDA_VER} does not support GCC ${GCC_VER}."
            echo "[GPU] Requires CUDA >= 12.2 for GCC 13+. Falling back to CPU."
        else
            TS_USE_CUDA=ON
            CUDA_EXTRA_FLAGS="--allow-unsupported-compiler"
            if [ -n "$BEST_CUDA_DIR" ]; then
                CUDA_ROOT_FLAG="-DCUDA_TOOLKIT_ROOT_DIR=${BEST_CUDA_DIR}"
            fi
            echo "[GPU] CUDA build ENABLED (${CUDA_VER} + GCC ${GCC_VER})"
        fi
    else
        echo "[GPU] nvcc not found — CUDA not available. Building CPU-only."
    fi
fi

echo "============================================"
echo " SeetaFace6 Build Script"
echo " Source: ${SCRIPT_DIR}"
echo " Output: ${BUILD_ROOT}"
echo " Threads: ${NPROC}"
echo " CUDA:    ${TS_USE_CUDA}"
echo "============================================"

# Clean if requested
if [ "$1" = "--clean" ]; then
    echo "[CLEAN] Removing previous build..."
    rm -rf "${BUILD_ROOT}"
fi

mkdir -p "${BUILD_ROOT}/lib"
mkdir -p "${BUILD_ROOT}/include/seeta"

# ============================================================
# 1. Build OpenRoleZoo (static library: libORZ_static.a)
# ============================================================
echo ""
echo "[1/7] Building OpenRoleZoo..."
ORZ_DIR="${SCRIPT_DIR}/OpenRoleZoo"
ORZ_BUILD="${BUILD_ROOT}/orz_build"
mkdir -p "${ORZ_BUILD}"
cd "${ORZ_BUILD}"
cmake "${ORZ_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM=auto \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    2>&1 | tail -5
make -j${NPROC} 2>&1 | tail -3
make install 2>&1 | tail -3

# Copy ORZ headers and libs
cp -r "${ORZ_DIR}/include/"* "${BUILD_ROOT}/include/" 2>/dev/null || true
find "${ORZ_BUILD}" -name "libORZ_static*" -exec cp {} "${BUILD_ROOT}/lib/" \; 2>/dev/null || true
find "${ORZ_DIR}" -name "libORZ_static*" -exec cp {} "${BUILD_ROOT}/lib/" \; 2>/dev/null || true
echo "[1/7] OpenRoleZoo DONE"

# ============================================================
# 2. Build SeetaAuthorize
# ============================================================
echo ""
echo "[2/7] Building SeetaAuthorize..."
SA_DIR="${SCRIPT_DIR}/SeetaAuthorize"
SA_BUILD="${BUILD_ROOT}/sa_build"
mkdir -p "${SA_BUILD}"
cd "${SA_BUILD}"
cmake "${SA_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM=auto \
    -DORZ_ROOT_DIR="${INSTALL_DIR}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    2>&1 | tail -5
make -j${NPROC} 2>&1 | tail -3
make install 2>&1 | tail -3

# Copy SeetaAuthorize headers and libs
cp -r "${SA_DIR}/include/"* "${BUILD_ROOT}/include/" 2>/dev/null || true
find "${SA_BUILD}" "${SA_DIR}" -name "libSeetaAuthorize*" -exec cp {} "${BUILD_ROOT}/lib/" \; 2>/dev/null || true
echo "[2/7] SeetaAuthorize DONE"

# ============================================================
# 3. Build TenniS (inference engine)
# ============================================================
echo ""
echo "[3/7] Building TenniS..."
TS_DIR="${SCRIPT_DIR}/TenniS"
TS_BUILD="${BUILD_ROOT}/ts_build"
mkdir -p "${TS_BUILD}"
cd "${TS_BUILD}"
cmake "${TS_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM=auto \
    -DTS_USE_OPENMP=ON \
    -DTS_USE_SIMD=ON \
    -DTS_ON_HASWELL=ON \
    -DTS_USE_CUDA=${TS_USE_CUDA} \
    -DCUDA_NVCC_FLAGS="${CUDA_EXTRA_FLAGS}" \
    ${CUDA_ROOT_FLAG} \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    2>&1 | tail -5
make -j${NPROC} 2>&1 | tail -3
make install 2>&1 | tail -3

# Copy TenniS headers and libs
cp -r "${TS_DIR}/include/api/"* "${BUILD_ROOT}/include/" 2>/dev/null || true
find "${TS_BUILD}" "${TS_DIR}" -name "libtennis*" -exec cp {} "${BUILD_ROOT}/lib/" \; 2>/dev/null || true
# Copy TenniS cmake find module for downstream modules
mkdir -p "${BUILD_ROOT}/cmake"
cp "${TS_DIR}/cmake/FindTenniS.cmake" "${BUILD_ROOT}/cmake/" 2>/dev/null || true
echo "[3/7] TenniS DONE"

# ============================================================
# Helper function to build SDK modules
# ============================================================
build_sdk_module() {
    local MODULE_NUM=$1
    local MODULE_NAME=$2
    local MODULE_DIR=$3
    
    echo ""
    echo "[${MODULE_NUM}/7] Building ${MODULE_NAME}..."
    local MOD_BUILD="${BUILD_ROOT}/${MODULE_NAME,,}_build"
    mkdir -p "${MOD_BUILD}"
    cd "${MOD_BUILD}"
    
    cmake "${MODULE_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPLATFORM=auto \
        -DSEETA_AUTHORIZE=ON \
        -DSEETA_MODEL_ENCRYPT=ON \
        -DCMAKE_MODULE_PATH="${BUILD_ROOT}/cmake;${INSTALL_DIR}/cmake" \
        -DCMAKE_PREFIX_PATH="${BUILD_ROOT}/cmake;${INSTALL_DIR}/cmake;${INSTALL_DIR}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DORZ_ROOT_DIR="${INSTALL_DIR}" \
        2>&1 | tail -5
    make -j${NPROC} 2>&1 | tail -3
    make install 2>&1 | tail -3
    
    # Copy headers - search in standard locations
    for inc_dir in $(find "${MODULE_DIR}" -path "*/include/seeta" -type d); do
        cp "${inc_dir}"/*.h "${BUILD_ROOT}/include/seeta/" 2>/dev/null || true
        if [ -d "${inc_dir}/Common" ]; then
            mkdir -p "${BUILD_ROOT}/include/seeta/Common"
            cp "${inc_dir}/Common/"*.h "${BUILD_ROOT}/include/seeta/Common/" 2>/dev/null || true
        fi
    done
    
    # Copy built libraries
    find "${MOD_BUILD}" "${MODULE_DIR}" -maxdepth 3 \
        \( -name "libSeeta*.so*" -o -name "libSeeta*.a" \) \
        -exec cp {} "${BUILD_ROOT}/lib/" \; 2>/dev/null || true
    
    echo "[${MODULE_NUM}/7] ${MODULE_NAME} DONE"
}

# ============================================================
# 4-7. Build SDK Modules
# ============================================================
build_sdk_module 4 "FaceDetector"    "${SCRIPT_DIR}/FaceBoxes"
build_sdk_module 5 "FaceLandmarker"  "${SCRIPT_DIR}/Landmarker"
build_sdk_module 6 "FaceRecognizer"  "${SCRIPT_DIR}/FaceRecognizer6"
build_sdk_module 7 "QualityAssessor" "${SCRIPT_DIR}/QualityAssessor3"
build_sdk_module 8 "MaskDetector"    "${SCRIPT_DIR}/SeetaMaskDetector"

# ============================================================
# Summary
# ============================================================
echo ""
echo "============================================"
echo " Build Complete!"
echo "============================================"
echo " Libraries:"
ls -la "${BUILD_ROOT}/lib/" 2>/dev/null | grep -E "\.(so|a)" || echo "  (none found)"
echo ""
echo " Headers:"
ls "${BUILD_ROOT}/include/seeta/"*.h 2>/dev/null | head -20 || echo "  (none found)"
echo ""
echo " To use in edgeos-sdk, build with:"
echo "   cmake .. -DCVEDIX_WITH_SEETAFACE=ON"
echo "============================================"
