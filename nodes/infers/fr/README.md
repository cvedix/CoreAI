# Face Recognition Module (FR)

## Structure

```
fr/
├── cvedix_face_recognition_node.*      # Unified node (auto backend selection)
├── cvedix_face_recognition_ort_node.*  # ONNX Runtime node
├── cvedix_face_recognition_trt_node.*  # TensorRT node
├── cvedix_face_registration_node.*     # Registration with augmentation
├── face_recognition_backend.h          # Backend interface
└── backends/
    ├── opencv_dnn_backend.h            # OpenCV DNN (always available)
    ├── onnx_runtime_backend.h          # ONNX Runtime
    └── tensorrt_backend.h              # TensorRT
```

## Usage

```cpp
#include <cvedix/nodes/infers/fr/cvedix_face_recognition_node.h>

// Auto-select best backend (TensorRT > ONNX Runtime > OpenCV DNN)
auto node = std::make_shared<cvedix_face_recognition_node>(
    "face_rec", "model.onnx", "database.txt"
);
```
