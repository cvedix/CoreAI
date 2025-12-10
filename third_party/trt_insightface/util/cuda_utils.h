#ifndef TRT_INSIGHTFACE_CUDA_UTILS_H_
#define TRT_INSIGHTFACE_CUDA_UTILS_H_

#include <cuda_runtime_api.h>
#include <iostream>
#include <cassert>

#ifndef CUDA_CHECK
#define CUDA_CHECK(callstr)\
    {\
        cudaError_t error_code = callstr;\
        if (error_code != cudaSuccess) {\
            const char* error_string = cudaGetErrorString(error_code);\
            std::cerr << "CUDA error " << error_code << " (" << error_string << ") at " << __FILE__ << ":" << __LINE__ << std::endl;\
            std::cerr << "Please check:" << std::endl;\
            std::cerr << "  1. GPU is properly installed and detected (run 'nvidia-smi')" << std::endl;\
            std::cerr << "  2. CUDA driver version is compatible" << std::endl;\
            std::cerr << "  3. GPU is not in an error state" << std::endl;\
            assert(0);\
        }\
    }
#endif  // CUDA_CHECK

#endif  // TRT_INSIGHTFACE_CUDA_UTILS_H_






