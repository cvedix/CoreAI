cmake_minimum_required(VERSION 3.10)
project(paddle_ocr VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -w -fdiagnostics-color=always -pthread")
# save all libs to the same directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)

# PaddlePaddle required  (please refer to 'env/install_paddle_inference.sh')
# Check if local paddle_inference exists in third_party
# Support both old structure (lib/, include/) and new GPU structure (paddle/lib, paddle/include)
set(PADDLE_INFERENCE_DIR "${CMAKE_SOURCE_DIR}/third_party/paddle_inference")

if(EXISTS "${PADDLE_INFERENCE_DIR}/paddle/lib")
    # New GPU v3.0 structure
    set(PADDLE_LIB_PATH "${PADDLE_INFERENCE_DIR}/paddle/lib")
    set(PADDLE_INC_PATH "${PADDLE_INFERENCE_DIR}/paddle/include")
    set(PADDLE_IS_GPU ON)
    message(STATUS "Using Paddle Inference GPU v3.0 structure")
elseif(EXISTS "${PADDLE_INFERENCE_DIR}/lib")
    # Old CPU structure
    set(PADDLE_LIB_PATH "${PADDLE_INFERENCE_DIR}/lib")
    set(PADDLE_INC_PATH "${PADDLE_INFERENCE_DIR}/include")
    set(PADDLE_IS_GPU OFF)
    message(STATUS "Using Paddle Inference CPU structure")
else()
    set(PADDLE_LIB_PATH "/usr/local/lib")      # change this line if possible
    set(PADDLE_INC_PATH "/usr/local/include")  # change this line if possible
    set(PADDLE_IS_GPU OFF)
endif()

message(STATUS "PaddlePaddle library status:")
message(STATUS "    include path: ${PADDLE_INC_PATH}")
message(STATUS "    library path: ${PADDLE_LIB_PATH}")

link_directories(${PADDLE_LIB_PATH})     
include_directories(${PADDLE_INC_PATH})

# Add dependencies include/lib paths - support both structures
# New GPU v3.0: third_party/install/<dep>/include and third_party/install/<dep>/lib
# Old CPU: <dep>/include and <dep>/lib (at root level)
set(GPU_DEPS_DIR "${PADDLE_INFERENCE_DIR}/third_party/install")
set(CPU_DEPS_DIR "${PADDLE_INFERENCE_DIR}")

# GLOG
if(EXISTS "${GPU_DEPS_DIR}/glog/include")
    include_directories("${GPU_DEPS_DIR}/glog/include")
    link_directories("${GPU_DEPS_DIR}/glog/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/glog/include")
    include_directories("${CPU_DEPS_DIR}/glog/include")
    link_directories("${CPU_DEPS_DIR}/glog/lib")
endif()

# GFLAGS
if(EXISTS "${GPU_DEPS_DIR}/gflags/include")
    include_directories("${GPU_DEPS_DIR}/gflags/include")
    link_directories("${GPU_DEPS_DIR}/gflags/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/gflags/include")
    include_directories("${CPU_DEPS_DIR}/gflags/include")
    link_directories("${CPU_DEPS_DIR}/gflags/lib")
endif()

# OneDNN (MKLDNN) - for CPU
if(EXISTS "${GPU_DEPS_DIR}/onednn/include")
    include_directories("${GPU_DEPS_DIR}/onednn/include")
    link_directories("${GPU_DEPS_DIR}/onednn/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/mkldnn/include")
    include_directories("${CPU_DEPS_DIR}/mkldnn/include")
    link_directories("${CPU_DEPS_DIR}/mkldnn/lib")
endif()

# Protobuf
if(EXISTS "${GPU_DEPS_DIR}/protobuf/include")
    include_directories("${GPU_DEPS_DIR}/protobuf/include")
    link_directories("${GPU_DEPS_DIR}/protobuf/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/protobuf/include")
    include_directories("${CPU_DEPS_DIR}/protobuf/include")
    link_directories("${CPU_DEPS_DIR}/protobuf/lib")
endif()

# xxhash
if(EXISTS "${GPU_DEPS_DIR}/xxhash/include")
    include_directories("${GPU_DEPS_DIR}/xxhash/include")
    link_directories("${GPU_DEPS_DIR}/xxhash/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/xxhash/include")
    include_directories("${CPU_DEPS_DIR}/xxhash/lib")
endif()

# MKLML - for CPU
if(EXISTS "${GPU_DEPS_DIR}/mklml/include")
    include_directories("${GPU_DEPS_DIR}/mklml/include")
    link_directories("${GPU_DEPS_DIR}/mklml/lib")
elseif(EXISTS "${CPU_DEPS_DIR}/mklml/include")
    include_directories("${CPU_DEPS_DIR}/mklml/include")
    link_directories("${CPU_DEPS_DIR}/mklml/lib")
endif()

# paddle2onnx - for CPU only
if(EXISTS "${CPU_DEPS_DIR}/paddle2onnx/lib")
    link_directories("${CPU_DEPS_DIR}/paddle2onnx/lib")
endif()

# onnxruntime - for CPU only
if(EXISTS "${CPU_DEPS_DIR}/onnxruntime/lib")
    link_directories("${CPU_DEPS_DIR}/onnxruntime/lib")
endif()

if(DEFINED VP_BUILD_FROM) # tell sdk where is paddle
    set(VP_PADDLE_LIB_PATH ${PADDLE_LIB_PATH} PARENT_SCOPE)     
    set(VP_PADDLE_INC_PATH ${PADDLE_INC_PATH} PARENT_SCOPE) 
endif()

# OpenCV required
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
include_directories(${OpenCV_INCLUDE_DIRS})

# Collect PADDLE dependent libs (different for GPU vs CPU)
if(PADDLE_IS_GPU)
    # GPU version: fewer dependencies, no onnxruntime/paddle2onnx
    set(PADDLE_LIBS paddle_inference phi_gpu phi_core gflags glog)
else()
    # CPU version: with onnxruntime support
    set(PADDLE_LIBS paddle_inference gflags glog paddle2onnx onnxruntime dnnl iomp5)
endif()
set(PADDLE_OCR_DEPEND_LIBS ${OpenCV_LIBS} ${PADDLE_LIBS} stdc++fs)

# collect source files for trt_vehicle
file(GLOB_RECURSE SRCS "src/*.cpp")
#...#
list(APPEND PADDLE_OCR_CPPS ${SRCS})

# build for paddle_ocr
add_library(${PROJECT_NAME} SHARED ${PADDLE_OCR_CPPS})
target_link_libraries(${PROJECT_NAME} ${PADDLE_OCR_DEPEND_LIBS})

# build samples for paddle_ocr
if(NOT DEFINED VP_BUILD_FROM)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/samples)  # save all exe to 'samples'
    add_executable(pd_ocr "samples/pd_ocr.cpp")
    target_link_libraries(pd_ocr ${PROJECT_NAME})
endif()

# Install paddle_ocr with export
install(TARGETS ${PROJECT_NAME} EXPORT cvedix-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)