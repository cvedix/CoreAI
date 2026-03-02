#!/bin/bash
# URL for Paddle Inference 2.6.0 (CPU) - Verified working URL
DOWNLOAD_URL="https://paddle-inference-lib.bj.bcebos.com/2.6.0/cxx_c/Linux/CPU/gcc8.2_avx_mkl/paddle_inference.tgz"

echo "Downloading Paddle Inference (CPU) from $DOWNLOAD_URL..."
wget -nc -O paddle_inference.tgz "$DOWNLOAD_URL"

echo "Extracting..."
tar -xf paddle_inference.tgz
rm paddle_inference.tgz

mkdir -p third_party/paddle_inference
# Correct structure based on tar content check:
# paddle_inference/paddle/include
# paddle_inference/paddle/lib
# paddle_inference/third_party
cp -r paddle_inference/paddle/* third_party/paddle_inference/
cp -r paddle_inference/third_party/install/* third_party/paddle_inference/

rm -rf paddle_inference

echo "Paddle Inference library downloaded to third_party/paddle_inference"
