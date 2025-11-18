# Inference Base Classes (infers/base/)

## Overview

This directory contains the base classes for all inference nodes in the pipeline. Inference nodes use deep learning models to perform tasks like object detection, classification, feature extraction, and more.

**Note**: These are abstract base classes. Concrete inference implementations are in the parent `../infers/` directory (e.g., `cvedix_yolo_detector_node`, `cvedix_classifier_node`).

## Base Classes

### cvedix_infer_node

Root base class for all inference nodes. Inherits from `cvedix_node`.

**Key Features**:
- Based on OpenCV DNN module (default backend)
- Supports other backends like TensorRT, PaddlePaddle (see concrete implementations)
- Manages model loading, preprocessing, inference, and postprocessing
- Handles batch processing for efficiency

**Inference Types**:
- **PRIMARY**: Inference on whole frames (e.g., detectors, pose estimation)
- **SECONDARY**: Inference on cropped regions (e.g., classifiers, feature extractors)

**Key Members**:
- `model_path`, `model_config_path`: Model file locations
- `labels_path`: Optional class labels file
- `input_width`, `input_height`: Model input dimensions
- `batch_size`: Batch size for inference
- `mean`, `std`, `scale`: Preprocessing parameters
- `swap_rb`: Whether to swap Red/Blue channels
- `swap_chn`: Whether to transpose channels (NCHW ↔ NHWC)

**Protected Methods**:
- `load_labels()`: Loads class labels if provided
- `preprocess()`: Prepares input for model (override in derived classes)
- `inference()`: Runs model inference (override in derived classes)
- `postprocess()`: Processes model output (override in derived classes)

### cvedix_primary_infer_node

Base class for primary inference nodes (inference on whole frames).

**Typical Use Cases**:
- Object detection (YOLO, Mask R-CNN)
- Pose estimation (OpenPose)
- Lane detection
- Face detection

**Characteristics**:
- Receives full frames from upstream
- Processes entire frame through model
- Outputs detections/features for the whole frame

### cvedix_secondary_infer_node

Base class for secondary inference nodes (inference on cropped regions).

**Typical Use Cases**:
- Object classification
- Feature extraction
- Attribute recognition
- Fine-grained detection on regions

**Characteristics**:
- Receives frames with regions of interest (ROIs) or crops
- Processes each ROI/crop through model
- Outputs results per region

## Creating a New Inference Node

### Primary Inference Node

1. Inherit from `cvedix_primary_infer_node`:
   ```cpp
   #include "cvedix_primary_infer_node.h"
   
   class my_detector_node : public cvedix_primary_infer_node {
       // ...
   };
   ```

2. Call base constructor with model paths and parameters:
   ```cpp
   my_detector_node::my_detector_node(
       std::string node_name,
       std::string model_path,
       std::string config_path = "",
       std::string labels_path = "",
       int input_w = 640, int input_h = 640)
       : cvedix_primary_infer_node(node_name, model_path, config_path, 
                                    labels_path, input_w, input_h) {
       // Load model, initialize
   }
   ```

3. Implement preprocessing:
   ```cpp
   cv::Mat my_detector_node::preprocess(const cv::Mat& frame) {
       // Resize, normalize, convert format
   }
   ```

4. Implement inference:
   ```cpp
   void my_detector_node::inference(const cv::Mat& input, 
                                    std::vector<cv::Mat>& outputs) {
       // Run model, get outputs
   }
   ```

5. Implement postprocessing:
   ```cpp
   void my_detector_node::postprocess(
       const std::vector<cv::Mat>& outputs,
       std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
       // Parse outputs, create detections, attach to meta
   }
   ```

### Secondary Inference Node

Similar process but inherit from `cvedix_secondary_infer_node` and process ROIs/crops instead of full frames.

## Backend Support

The base classes are designed for OpenCV DNN, but you can implement other backends:

- **TensorRT**: See `cvedix_trt_vehicle_detector` in `../infers/`
- **PaddlePaddle**: See `cvedix_ppocr_text_detector_node` in `../infers/`
- **OpenCV DNN**: Default, supports ONNX, TensorFlow, Caffe, etc.

## Related Files

- Node base: `../../common/cvedix_node.h`
- Concrete implementations: `../` directory (various detector/classifier nodes)

