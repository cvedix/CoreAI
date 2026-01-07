#!/bin/bash
# Script to download PaddleOCR v3 models for cvedix_plate_recogniton_ppocr3

# Define paths
DATA_ROOT="./cvedix_data/models/paddle/ocr"
mkdir -p $DATA_ROOT

echo "Downloading PaddleOCR models to $DATA_ROOT..."

# 1. Detection Model (ch_PP-OCRv3_det_infer)
if [ ! -d "$DATA_ROOT/ch_PP-OCRv3_det_infer" ]; then
    echo "Downloading Detection Model..."
    wget -nc https://paddleocr.bj.bcebos.com/PP-OCRv3/chinese/ch_PP-OCRv3_det_infer.tar -P $DATA_ROOT
    tar -xf $DATA_ROOT/ch_PP-OCRv3_det_infer.tar -C $DATA_ROOT
    rm $DATA_ROOT/ch_PP-OCRv3_det_infer.tar
else
    echo "Detection Model already exists."
fi

# 2. Classification Model (ch_ppocr_mobile_v2.0_cls_infer)
if [ ! -d "$DATA_ROOT/ch_ppocr_mobile_v2.0_cls_infer" ]; then
    echo "Downloading Classification Model..."
    wget -nc https://paddleocr.bj.bcebos.com/dygraph_v2.0/ch/ch_ppocr_mobile_v2.0_cls_infer.tar -P $DATA_ROOT
    tar -xf $DATA_ROOT/ch_ppocr_mobile_v2.0_cls_infer.tar -C $DATA_ROOT
    rm $DATA_ROOT/ch_ppocr_mobile_v2.0_cls_infer.tar
else
    echo "Classification Model already exists."
fi

# 3. Recognition Model (ch_PP-OCRv3_rec_infer)
if [ ! -d "$DATA_ROOT/ch_PP-OCRv3_rec_infer" ]; then
    echo "Downloading Recognition Model..."
    wget -nc https://paddleocr.bj.bcebos.com/PP-OCRv3/chinese/ch_PP-OCRv3_rec_infer.tar -P $DATA_ROOT
    tar -xf $DATA_ROOT/ch_PP-OCRv3_rec_infer.tar -C $DATA_ROOT
    rm $DATA_ROOT/ch_PP-OCRv3_rec_infer.tar
else
    echo "Recognition Model already exists."
fi

# 4. Dictionary Keys (ppocr_keys_v1.txt)
if [ ! -f "$DATA_ROOT/ppocr_keys_v1.txt" ]; then
    echo "Downloading Dictionary Keys..."
    wget -nc https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/release/2.6/ppocr/utils/ppocr_keys_v1.txt -P $DATA_ROOT
else
    echo "Dictionary Keys already exists."
fi

echo "All models downloaded successfully!"
echo "Path: $DATA_ROOT"
ls -F $DATA_ROOT
