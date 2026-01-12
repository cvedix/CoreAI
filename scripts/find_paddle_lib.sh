#!/bin/bash

BASE="https://paddle-inference-lib.bj.bcebos.com"
# Variations
VERSIONS=("2.6.0" "2.6.1")
TRTS=("trt8.5.1.7" "trt8.5.2.2" "trt8.5.3.4" "trt8.6.1.6")

for VER in "${VERSIONS[@]}"; do
    for TRT in "${TRTS[@]}"; do
        URL="$BASE/$VER/cxx_c/Linux/GPU/x86-64_gcc8.2_avx_mkl_cuda11.8_cudnn8.6.0_$TRT/paddle_inference.tgz"
        echo "Checking $URL..."
        if curl --output /dev/null --silent --head --fail "$URL"; then
            echo "FOUND: $URL"
            exit 0
        fi
    done
done

echo "Not found."
exit 1
