#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_SAMPLES="${BUILD_SAMPLES:-ON}"
WITH_LLM="${WITH_LLM:-ON}"
ONNXRUNTIME_DIR="${ONNXRUNTIME_DIR:-${ROOT_DIR}/third_party/onnxruntime}"
OPENVINO_ROOT="${OPENVINO_ROOT:-}"

die() {
    echo "Error: $*" >&2
    exit 1
}

has_tensorrt() {
    [ -f /usr/include/x86_64-linux-gnu/NvInfer.h ] ||
    [ -f /usr/include/aarch64-linux-gnu/NvInfer.h ] ||
    [ -f /usr/local/tensorRT/include/NvInfer.h ]
}

detect_openvino_root() {
    if [ -n "${OPENVINO_ROOT}" ] && [ -f "${OPENVINO_ROOT}/setupvars.sh" ]; then
        echo "${OPENVINO_ROOT}"
        return
    fi

    if [ -f /opt/intel/openvino/setupvars.sh ]; then
        echo "/opt/intel/openvino"
        return
    fi

    local candidate
    for candidate in /opt/intel/openvino_* "${HOME}"/intel/openvino_*; do
        if [ -f "${candidate}/setupvars.sh" ]; then
            echo "${candidate}"
            return
        fi
    done
}

echo "Building OmniCore with NVIDIA CUDA/TensorRT + OpenVINO + ONNX Runtime"
echo "  Root:       ${ROOT_DIR}"
echo "  Build dir:  ${BUILD_DIR}"
echo "  Build type: ${BUILD_TYPE}"
echo "  Samples:    ${BUILD_SAMPLES}"
echo "  LLM:        ${WITH_LLM}"

command -v cmake >/dev/null 2>&1 || die "cmake is not installed"
command -v make >/dev/null 2>&1 || die "make is not installed"

[ -d /usr/local/cuda ] || die "CUDA not found at /usr/local/cuda"
has_tensorrt || die "TensorRT headers not found. Expected NvInfer.h under /usr/include/*-linux-gnu or /usr/local/tensorRT/include"

OPENVINO_ROOT="$(detect_openvino_root || true)"
if [ -n "${OPENVINO_ROOT}" ]; then
    # shellcheck source=/dev/null
    set +u
    source "${OPENVINO_ROOT}/setupvars.sh" >/dev/null 2>&1 || true
    set -u
    export OpenVINO_DIR="${OPENVINO_ROOT}/runtime/cmake"
    export CMAKE_PREFIX_PATH="${OPENVINO_ROOT}/runtime/cmake:${CMAKE_PREFIX_PATH:-}"
    echo "  OpenVINO:   ${OPENVINO_ROOT}"
else
    die "OpenVINO not found. Install it under /opt/intel/openvino or ~/intel/openvino_*, or set OPENVINO_ROOT=/path/to/openvino"
fi

[ -f "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so" ] || die "ONNX Runtime not found at ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so"
[ -d "${ONNXRUNTIME_DIR}/include" ] || die "ONNX Runtime headers not found at ${ONNXRUNTIME_DIR}/include"

mkdir -p "${ROOT_DIR}/${BUILD_DIR}"
cd "${ROOT_DIR}/${BUILD_DIR}"

cmake \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCVEDIX_WITH_CUDA=ON \
    -DCVEDIX_WITH_TRT=ON \
    -DCVEDIX_WITH_OPENVINO=ON \
    -DCVEDIX_WITH_LLM="${WITH_LLM}" \
    -DONNXRUNTIME_DIR="${ONNXRUNTIME_DIR}" \
    -DCVEDIX_BUILD_SAMPLES="${BUILD_SAMPLES}" \
    ..

make -j"$(nproc)"

echo "Build completed: ${ROOT_DIR}/${BUILD_DIR}"
echo "Sample binaries: ${ROOT_DIR}/${BUILD_DIR}/bin"
