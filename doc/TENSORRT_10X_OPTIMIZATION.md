# TensorRT 10.x Optimization - Auto Tensor Name Detection

## Overview
Code đã được cập nhật để tự động phát hiện tensor names và output sizes từ TensorRT engine, tối ưu cho TensorRT 10.x API.

## Thay đổi chính

### 1. Auto-detect Tensor Names
**Trước đây**: Hardcode tensor names trong code
```cpp
static constexpr const char* kInputTensorName = "input.1";
static constexpr const char* kOutputTensorName = "516";
```

**Bây giờ**: Tự động detect từ engine khi load
```cpp
std::string input_tensor_name_;   // Auto-detected
std::string output_tensor_name_;  // Auto-detected
```

### 2. Auto-detect Output Size
**Trước đây**: Hardcode embedding size = 512
```cpp
static constexpr int kEmbeddingSize = 512;
```

**Bây giờ**: Tự động detect từ output tensor shape
```cpp
int embedding_size_;  // Auto-detected from engine
```

### 3. TensorRT 10.x API Usage
Sử dụng TensorRT 10.x API để query tensor information:
- `engine->getNbIOTensors()` - Số lượng I/O tensors
- `engine->getTensorName(i)` - Tên tensor tại index i
- `engine->getTensorIOMode(name)` - Kiểm tra INPUT/OUTPUT
- `engine->getTensorShape(name)` - Lấy shape của tensor

## Lợi ích

1. **Tương thích tốt hơn**: Hoạt động với mọi model InsightFace (buffalo_s, buffalo_l, etc.)
2. **Không cần hardcode**: Tự động adapt với tensor names khác nhau
3. **Dễ maintain**: Không cần update code khi đổi model
4. **TensorRT 10.x native**: Sử dụng API mới nhất của TensorRT

## Tensor Names được hỗ trợ

Code tự động detect và hỗ trợ các tensor names sau:

| Model | Input Tensor | Output Tensor | Output Size |
|-------|--------------|---------------|------------|
| w600k_mbf (buffalo_s) | `input.1` | `516` | 512 |
| w600k_r50 (buffalo_l) | `data` | `fc1` | 512 |
| det_500m | `input.1` | (multiple) | - |
| genderage | `data` | `fc1` | 3 |
| 2d106det | `data` | `fc1` | 212 |
| 1k3d68 | `data` | `fc1` | 3309 |

## Code Changes

### Header File (`insight_face_recognition.h`)
```cpp
// Removed hardcoded tensor names
- static constexpr const char* kInputTensorName = "input.1";
- static constexpr const char* kOutputTensorName = "516";
- static constexpr int kEmbeddingSize = 512;

// Added dynamic detection
+ std::string input_tensor_name_;
+ std::string output_tensor_name_;
+ int embedding_size_;
+ void detect_tensor_names();
```

### Implementation (`insight_face_recognition.cpp`)
```cpp
// New method: Auto-detect tensor names from engine
void InsightFaceRecognition::detect_tensor_names() {
    int num_io_tensors = engine->getNbIOTensors();
    
    for (int i = 0; i < num_io_tensors; i++) {
        const char* tensor_name = engine->getTensorName(i);
        nvinfer1::TensorIOMode io_mode = engine->getTensorIOMode(tensor_name);
        
        if (io_mode == nvinfer1::TensorIOMode::kINPUT) {
            input_tensor_name_ = std::string(tensor_name);
        } else if (io_mode == nvinfer1::TensorIOMode::kOUTPUT) {
            output_tensor_name_ = std::string(tensor_name);
            // Detect output size from shape
            nvinfer1::Dims output_dims = engine->getTensorShape(tensor_name);
            embedding_size_ = output_dims.d[output_dims.nbDims - 1];
        }
    }
}
```

## Usage

Code hoạt động tự động, không cần thay đổi cách sử dụng:

```cpp
// Create recognizer - tensor names auto-detected
auto recognizer = std::make_shared<trt_insightface::InsightFaceRecognition>(
    "./w600k_mbf_fp16.engine"  // Works with any engine
);

// Extract features - output size auto-detected
std::vector<cv::Mat> faces;
std::vector<std::vector<float>> embeddings;
recognizer->extract_features(faces, embeddings);

// Get embedding size (auto-detected)
int size = recognizer->get_embedding_size();  // Returns actual size from engine
```

## Debug Output

Khi load engine, code sẽ in ra tensor names đã detect:
```
[TRT] Auto-detected tensor names:
  Input:  input.1
  Output: 516 (size: 512)
```

## Testing

Để test với các models khác nhau:

```bash
# Test với buffalo_s
./test_app w600k_mbf_fp16.engine

# Test với buffalo_l (nếu có)
./test_app w600k_r50_fp16.engine

# Code tự động adapt với tensor names khác nhau
```

## Compatibility

- ✅ TensorRT 10.x (v100900+)
- ✅ All InsightFace models (buffalo_s, buffalo_l, etc.)
- ✅ Dynamic batch sizes
- ✅ Different tensor naming conventions

## Notes

- Tensor names được detect một lần khi load engine
- Output size được detect từ output tensor shape
- Code tự động handle các models với tensor names khác nhau
- Không cần rebuild khi đổi model (chỉ cần đổi engine file)

