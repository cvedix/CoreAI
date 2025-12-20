/**
 * @file onnx_runtime_backend.h
 * @brief ONNX Runtime backend for face recognition (requires CVEDIX_WITH_ORT)
 */

#pragma once

#ifdef CVEDIX_WITH_ORT

#include "../face_recognition_backend.h"
#include <onnxruntime_cxx_api.h>

namespace cvedix_nodes {

/**
 * @brief ONNX Runtime backend implementation
 * 
 * Uses ONNX Runtime for inference with better performance than OpenCV DNN.
 * Supports models like glint360k_r100, w600k_r50.
 */
class OnnxRuntimeBackend : public IFaceRecognitionBackend {
public:
    OnnxRuntimeBackend(const std::string& model_path, int input_width = 112, int input_height = 112)
        : env_(ORT_LOGGING_LEVEL_WARNING, "FaceRecognition"),
          model_path_(model_path), input_width_(input_width), input_height_(input_height) {
        
        try {
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(4);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            
            session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options);
            
            // Get input info
            size_t num_inputs = session_->GetInputCount();
            for (size_t i = 0; i < num_inputs; i++) {
                auto name = session_->GetInputNameAllocated(i, allocator_);
                input_names_.push_back(strdup(name.get()));
                
                auto type_info = session_->GetInputTypeInfo(i);
                auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
                input_shape_ = tensor_info.GetShape();
            }
            
            // Get output info
            size_t num_outputs = session_->GetOutputCount();
            for (size_t i = 0; i < num_outputs; i++) {
                auto name = session_->GetOutputNameAllocated(i, allocator_);
                output_names_.push_back(strdup(name.get()));
                
                auto type_info = session_->GetOutputTypeInfo(i);
                auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
                auto shape = tensor_info.GetShape();
                if (shape.size() >= 2) {
                    embedding_dim_ = static_cast<int>(shape[1]);
                }
            }
            
            ready_ = true;
            CVEDIX_INFO("[OnnxRuntimeBackend] Loaded model: " + model_path + " (emb_dim=" + std::to_string(embedding_dim_) + ")");
            
        } catch (const std::exception& e) {
            CVEDIX_ERROR("[OnnxRuntimeBackend] Exception: " + std::string(e.what()));
        }
    }
    
    ~OnnxRuntimeBackend() {
        for (auto name : input_names_) free((void*)name);
        for (auto name : output_names_) free((void*)name);
    }
    
    std::vector<float> extractEmbedding(const cv::Mat& aligned_face) override {
        if (!ready_) {
            return std::vector<float>(embedding_dim_, 0.0f);
        }
        
        // Preprocess: normalize to [-1, 1]
        cv::Mat rgb;
        if (aligned_face.channels() == 3) {
            cv::cvtColor(aligned_face, rgb, cv::COLOR_BGR2RGB);
        } else {
            rgb = aligned_face;
        }
        
        cv::Mat resized;
        cv::resize(rgb, resized, cv::Size(input_width_, input_height_));
        
        cv::Mat float_img;
        resized.convertTo(float_img, CV_32FC3, 1.0f / 127.5f, -1.0f);
        
        // Prepare input tensor (NCHW format)
        std::vector<float> input_tensor(1 * 3 * input_height_ * input_width_);
        for (int c = 0; c < 3; c++) {
            for (int h = 0; h < input_height_; h++) {
                for (int w = 0; w < input_width_; w++) {
                    input_tensor[c * input_height_ * input_width_ + h * input_width_ + w] =
                        float_img.ptr<cv::Vec3f>(h)[w][c];
                }
            }
        }
        
        // Run inference
        std::vector<int64_t> input_shape = {1, 3, input_height_, input_width_};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor_ort = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor.data(), input_tensor.size(),
            input_shape.data(), input_shape.size()
        );
        
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_.data(), &input_tensor_ort, 1,
            output_names_.data(), output_names_.size()
        );
        
        // Extract embedding
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        std::vector<float> embedding(embedding_dim_);
        
        float norm = 0.0f;
        for (int i = 0; i < embedding_dim_; i++) {
            embedding[i] = output_data[i];
            norm += embedding[i] * embedding[i];
        }
        norm = std::sqrt(norm);
        if (norm > 1e-6f) {
            for (float& v : embedding) v /= norm;
        }
        
        return embedding;
    }
    
    int getEmbeddingDim() const override { return embedding_dim_; }
    std::string getBackendName() const override { return "ONNX_RUNTIME"; }
    bool isReady() const override { return ready_; }
    
private:
    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::AllocatorWithDefaultOptions allocator_;
    
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;
    
    std::string model_path_;
    int input_width_;
    int input_height_;
    int embedding_dim_ = 512;
    bool ready_ = false;
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_ORT
