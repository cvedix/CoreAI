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
            std::cerr << "CUDA error " << error_code << " at " << __FILE__ << ":" << __LINE__;\
            assert(0);\
        }\
    }
#endif  // CUDA_CHECK

#endif  // TRT_INSIGHTFACE_CUDA_UTILS_H_



