#!/bin/bash

export LD_LIBRARY_PATH=$(pwd)/build/libs:$(pwd)/third_party/onnxruntime/lib:$LD_LIBRARY_PATH

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Starting OmniCore Face Detection Web Debug Dashboard...   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "  Open http://localhost:9091 in your browser"
echo ""
echo "  Pipeline:"
echo "    file_src → face_yolov11 → sort → osd → web"
echo ""
echo "  Press Ctrl+C to stop"
echo ""

# Run the face detection TRT web debug sample
# Pass arguments through: [video_path] [model_path] [backend] [port]
./build/bin/face_detection_yolov11_trt_web_debug_sample "$@"
