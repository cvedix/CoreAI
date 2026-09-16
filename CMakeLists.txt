#[[
    prepare in advance：
    1. OpenCV >= 4.9
    2. Gstreamer >= 1.14.5
    3. CUDA, TensorRT, PADDLE and others for optional
]]

cmake_minimum_required(VERSION 3.10)
project(cvedix_instance_sdk VERSION 2025.0.1.3)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -w -fdiagnostics-color=always -pthread")
# save all libs(including third_party's) to 'libs'
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)

# optional for build, modify values when configure the project using 'cmake -DCVEDIX_WITH_CUDA=OFF ..'
option(CVEDIX_WITH_CUDA "prepared CUDA or not?" OFF)
option(CVEDIX_WITH_TRT "prepared TensorRT or not?" OFF)
option(CVEDIX_WITH_OPENVINO "prepared OpenVINO or not?" OFF)
option(CVEDIX_WITH_PADDLE "prepared PaddlePaddle or not?" OFF)
option(CVEDIX_WITH_LLM "Enable LLM support (HTTP mLLM client + local llama.cpp inference)?" OFF)
option(CVEDIX_LLAMACPP_WITH_CUDA "Build llama.cpp with CUDA backend when LLM is enabled?" ON)
option(CVEDIX_WITH_RKNN "prepared RKNN (Rockchip NPU) or not?" OFF)
option(CVEDIX_WITH_RGA "prepared RGA (Rockchip Graphics Accelerator) or not?" OFF)
option(CVEDIX_WITH_MILVUS "prepared Milvus vector DB for large-scale face search?" OFF)
set(CVEDIX_BUILD_SAMPLES ON)
option(CVEDIX_BUILD_COMPLEX_SAMPLES "build complex samples or not? (maybe source code not provided)" OFF)
option(CVEDIX_BUILD_CAPI "build C API layer (libcvedix_capi) for language bindings (Java/C#/C)?" ON)
option(CVEDIX_BUILD_TESTS "build unit/smoke tests (needs no models or GPU)?" OFF)

# OpenCV required
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
include_directories(${OpenCV_INCLUDE_DIRS})

# OpenCV contrib freetype module (for Chinese font support)
find_path(OPENCV_FREETYPE_INCLUDE_DIR
    NAMES opencv2/freetype.hpp
    PATHS /home/ubuntu/opencv_contrib/modules/freetype/include
          /usr/local/include
          /usr/local/include/opencv4
          /usr/include
          /usr/include/opencv4
    NO_DEFAULT_PATH
)
if(OPENCV_FREETYPE_INCLUDE_DIR)
    message(STATUS "OpenCV freetype module found: ${OPENCV_FREETYPE_INCLUDE_DIR}")
    include_directories(${OPENCV_FREETYPE_INCLUDE_DIR})
else()
    find_path(OPENCV_FREETYPE_INCLUDE_DIR NAMES opencv2/freetype.hpp)
    if(OPENCV_FREETYPE_INCLUDE_DIR)
        message(STATUS "OpenCV freetype module found: ${OPENCV_FREETYPE_INCLUDE_DIR}")
        include_directories(${OPENCV_FREETYPE_INCLUDE_DIR})
    else()
        message(WARNING "OpenCV freetype module not found. Some OSD nodes may not compile correctly.")
    endif()
endif()

# GStreamer mandatory
include(FindPkgConfig)
pkg_check_modules(GST   REQUIRED gstreamer-1.0)
pkg_check_modules(GSTAPP REQUIRED gstreamer-app-1.0)
# RTSP server is required
pkg_check_modules(GST_RTSP REQUIRED gstreamer-rtsp-server-1.0)
message(STATUS "GStreamer RTSP server found - RTSP nodes will be available")
message(STATUS "GStreamer library status:")
message(STATUS "    version: ${GST_VERSION}")
message(STATUS "    libraries: ${GST_LIBRARIES} ${GSTAPP_LIBRARIES} ${GST_RTSP_LIBRARIES}")
message(STATUS "    include path: ${GST_INCLUDE_DIRS}")
include_directories(${GST_INCLUDE_DIRS})
set (GST_DEPEND_LIBS ${GST_LIBRARIES} ${GSTAPP_LIBRARIES} ${GST_RTSP_LIBRARIES})
add_definitions(-DCVEDIX_WITH_GSTREAMER)

# collect dependent libs for SDK
list(APPEND CVEDIX_DEPEND_LIBS ${OpenCV_LIBS} ${GST_DEPEND_LIBS} stdc++fs)

# optional for CUDA
if(CVEDIX_WITH_CUDA) # CUDA enabled
    add_definitions(-DCVEDIX_WITH_CUDA)
endif()

# ---------------- IMPORTANT ---------------- 
# Build default plugin backend
# onnx by opencv this is default CPU
add_subdirectory(third_party/onnx_yolov11)
add_subdirectory(third_party/onnx_yolov26)

# Build for TensorRT Backend
if(CVEDIX_WITH_TRT)  # TensorRT enabled
    add_definitions(-DCVEDIX_WITH_TRT)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project

    # Auto-detect TensorRT: apt packages first, then manual install
    if(EXISTS "/usr/include/x86_64-linux-gnu/NvInfer.h")
        # TensorRT installed via apt (Debian/Ubuntu)
        set(CVEDIX_TRT_INC_PATH "/usr/include/x86_64-linux-gnu/")
        set(CVEDIX_TRT_LIB_PATH "/usr/lib/x86_64-linux-gnu/")
        message(STATUS "TensorRT detected: apt packages (x86_64)")
    elseif(EXISTS "/usr/include/aarch64-linux-gnu/NvInfer.h")
        # TensorRT installed via apt/JetPack on NVIDIA Jetson
        set(CVEDIX_TRT_INC_PATH "/usr/include/aarch64-linux-gnu/")
        set(CVEDIX_TRT_LIB_PATH "/usr/lib/aarch64-linux-gnu/")
        message(STATUS "TensorRT detected: apt packages (aarch64)")
    elseif(EXISTS "/usr/local/tensorRT/include/NvInfer.h")
        # TensorRT manual install
        set(CVEDIX_TRT_INC_PATH "/usr/local/tensorRT/include/")
        set(CVEDIX_TRT_LIB_PATH "/usr/local/tensorRT/lib/")
        message(STATUS "TensorRT detected: manual install at /usr/local/tensorRT")
    else()
        message(FATAL_ERROR "TensorRT not found! Install via apt or set paths manually.")
    endif()

    # Add TensorRT and CUDA include directories for main project
    include_directories(${CVEDIX_TRT_INC_PATH})
    link_directories(${CVEDIX_TRT_LIB_PATH})
    include_directories(/usr/local/cuda/include)
    link_directories(/usr/local/cuda/lib64)
    
    message(STATUS "TensorRT library status:")
    message(STATUS "    include path: ${CVEDIX_TRT_INC_PATH}")
    message(STATUS "    library path: ${CVEDIX_TRT_LIB_PATH}")

    # trt_yolov11 backend plugin
    message("-------------start build trt_yolov11--------------")
    add_subdirectory(third_party/trt_yolov11)
    message("--------------end build trt_yolov11---------------")

    message("-------------start build trt_yolov26--------------")
    add_subdirectory(third_party/trt_yolov26)
    message("--------------end build trt_yolov26---------------")

    message("-------------start build trt_yolov12--------------")
    add_subdirectory(third_party/trt_yolov12)
    message("--------------end build trt_yolov12---------------")

    message("-------------start build trt_rf_detr--------------")
    add_subdirectory(third_party/trt_rf_detr)
    message("--------------end build trt_rf_detr---------------")

    # trt_botsort
    message("-------------start build trt_botsort--------------")
    add_subdirectory(third_party/trt_botsort)
    list(APPEND CVEDIX_DEPEND_LIBS trt_botsort)
    message("--------------end build trt_botsort---------------")

endif()

# Build for OpenVINO Backend
if(CVEDIX_WITH_OPENVINO)
    add_definitions(-DCVEDIX_WITH_OPENVINO)
    message("--------------start build ov_yolov11--------------")
    add_subdirectory(third_party/ov_yolov11)
    message("--------------end build ov_yolov11---------------")
    message("--------------start build ov_yolov26--------------")
    add_subdirectory(third_party/ov_yolov26)
    message("--------------end build ov_yolov26---------------")
endif()

# optional for PaddlePaddle
if(CVEDIX_WITH_PADDLE)  # PaddlePaddle enabled
    add_definitions(-DCVEDIX_WITH_PADDLE)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project
    set(VP_BUILD_FROM ON)      # set flag for paddle_ocr sub project
    set(CVEDIX_PADDLE_ROOT "/opt/paddle_inference" CACHE PATH "Paddle Inference root")
    if(NOT EXISTS ${CVEDIX_PADDLE_ROOT} AND EXISTS "/workspace/paddle_inference")
        set(CVEDIX_PADDLE_ROOT "/workspace/paddle_inference")
    endif()
    set(CVEDIX_PADDLE_LIB_PATH "${CVEDIX_PADDLE_ROOT}/paddle/lib")
    set(CVEDIX_PADDLE_INC_PATH "${CVEDIX_PADDLE_ROOT}/paddle/include")
    set(CVEDIX_PADDLE_THIRD_PARTY_PATH "${CVEDIX_PADDLE_ROOT}/third_party/install")
    # message("-------------start build paddle_ocr--------------")
    # add_subdirectory(third_party/paddle_ocr) # build paddle_ocr
    # list(APPEND CVEDIX_DEPEND_LIBS paddle_ocr)
    
    # Use variables set by sub-project
    if(DEFINED VP_PADDLE_LIB_PATH)
        set(CVEDIX_PADDLE_LIB_PATH ${VP_PADDLE_LIB_PATH})
    endif()
    if(DEFINED VP_PADDLE_INC_PATH)
        set(CVEDIX_PADDLE_INC_PATH ${VP_PADDLE_INC_PATH})
    endif()

    link_directories(${CVEDIX_PADDLE_LIB_PATH})
    include_directories(${CVEDIX_PADDLE_INC_PATH})
    include_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/glog/include)
    include_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/gflags/include)
    include_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/protobuf/include)
    include_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/xxhash/include)
    include_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/yaml-cpp/include)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/paddle_detection)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/paddle_detection/include)

    # Add dependencies library paths (propagate from paddle_inference path)
    # Support both old CPU structure and new GPU v3.0 structure
    set(PADDLE_INF_DIR "${CVEDIX_PADDLE_ROOT}")
    set(GPU_DEPS_DIR "${PADDLE_INF_DIR}/third_party/install")
    
    # Check for GPU v3.0 structure first, then fallback to CPU structure
    if(EXISTS "${GPU_DEPS_DIR}/glog/lib")
        link_directories("${GPU_DEPS_DIR}/glog/lib")
        link_directories("${GPU_DEPS_DIR}/gflags/lib")
        link_directories("${GPU_DEPS_DIR}/protobuf/lib")
        link_directories("${GPU_DEPS_DIR}/xxhash/lib")
        link_directories("${GPU_DEPS_DIR}/mklml/lib")
        link_directories("${GPU_DEPS_DIR}/onednn/lib")
        message(STATUS "Using Paddle GPU v3.0 dependencies from third_party/install/")
    else()
        # Old CPU structure
        if(EXISTS "${PADDLE_INF_DIR}/glog/lib")
            link_directories("${PADDLE_INF_DIR}/glog/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/gflags/lib")
            link_directories("${PADDLE_INF_DIR}/gflags/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/mkldnn/lib")
            link_directories("${PADDLE_INF_DIR}/mkldnn/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/protobuf/lib")
            link_directories("${PADDLE_INF_DIR}/protobuf/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/xxhash/lib")
            link_directories("${PADDLE_INF_DIR}/xxhash/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/paddle2onnx/lib")
            link_directories("${PADDLE_INF_DIR}/paddle2onnx/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/onnxruntime/lib")
            link_directories("${PADDLE_INF_DIR}/onnxruntime/lib")
        endif()
        if(EXISTS "${PADDLE_INF_DIR}/mklml/lib")
            link_directories("${PADDLE_INF_DIR}/mklml/lib")
        endif()
    endif()
    
    link_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/glog/lib)
    link_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/gflags/lib)
    link_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/protobuf/lib)
    link_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/xxhash/lib)
    link_directories(${CVEDIX_PADDLE_THIRD_PARTY_PATH}/yaml-cpp/lib)

    list(APPEND CVEDIX_DEPEND_LIBS paddle_inference yaml-cpp glog gflags protobuf xxhash)

    set(CVEDIX_PADDLE_DETECTION_SRCS
        ${CMAKE_SOURCE_DIR}/third_party/paddle_detection/src/object_detector.cc
        ${CMAKE_SOURCE_DIR}/third_party/paddle_detection/src/preprocess_op.cc
        ${CMAKE_SOURCE_DIR}/third_party/paddle_detection/src/picodet_postprocess.cc
        ${CMAKE_SOURCE_DIR}/third_party/paddle_detection/src/utils.cc
    )

    message(STATUS "PaddlePaddle library status:")
    message(STATUS "    include path: ${CVEDIX_PADDLE_INC_PATH}")
    message(STATUS "    library path: ${CVEDIX_PADDLE_LIB_PATH}")
    message(STATUS "    root path: ${CVEDIX_PADDLE_ROOT}")
    message("--------------end build paddle_ocr---------------")
endif()

# Kafka mandatory (librdkafka)
find_library(RDKAFKA_LIB
    NAMES rdkafka++
    PATHS /usr/lib /usr/local/lib /opt/lib
)

find_path(RDKAFKA_INCLUDE_DIR
    NAMES librdkafka/rdkafkacpp.h
    PATHS /usr/include /usr/local/include /opt/include
)

if(NOT RDKAFKA_LIB OR NOT RDKAFKA_INCLUDE_DIR)
    message(FATAL_ERROR "\n"
        "========================================\n"
        "librdkafka not found!\n"
        "========================================\n"
        "librdkafka is a required dependency.\n"
        "\n"
        "To install librdkafka development packages:\n"
        "  Ubuntu/Debian: sudo apt-get install librdkafka-dev\n"
        "  Fedora/RHEL:   sudo dnf install librdkafka-devel\n"
        "  Arch Linux:    sudo pacman -S librdkafka\n"
        "========================================\n"
    )
endif()

message(STATUS "librdkafka library status:")
message(STATUS "    library: ${RDKAFKA_LIB}")
message(STATUS "    include path: ${RDKAFKA_INCLUDE_DIR}")
include_directories(${RDKAFKA_INCLUDE_DIR})
list(APPEND CVEDIX_DEPEND_LIBS ${RDKAFKA_LIB})
add_definitions(-DCVEDIX_WITH_KAFKA)
message(STATUS "Kafka support: enabled")

# optional for LLM
if(CVEDIX_WITH_LLM)
    # Try to find OpenSSL
    find_package(OpenSSL)
    
    # Check if OpenSSL was found (OpenSSL_FOUND is set by find_package)
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR "\n"
            "========================================\n"
            "OpenSSL not found!\n"
            "========================================\n"
            "OpenSSL is required when CVEDIX_WITH_LLM=ON\n"
            "\n"
            "To install OpenSSL development packages:\n"
            "  Ubuntu/Debian: sudo apt-get install libssl-dev\n"
            "  Fedora/RHEL:   sudo dnf install openssl-devel\n"
            "  Arch Linux:    sudo pacman -S openssl\n"
            "\n"
            "Or disable LLM support:\n"
            "  cmake -DCVEDIX_WITH_LLM=OFF ..\n"
            "========================================\n"
        )
    endif()
    
    message(STATUS "OpenSSL library status:")
    message(STATUS "    version: ${OPENSSL_VERSION}")
    message(STATUS "    libraries: ${OPENSSL_LIBRARIES}")
    message(STATUS "    include path: ${OPENSSL_INCLUDE_DIR}")
    include_directories(${OPENSSL_INCLUDE_DIR})
    list(APPEND CVEDIX_DEPEND_LIBS ${OPENSSL_LIBRARIES})
    add_definitions(-DCVEDIX_WITH_LLM)
    add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
endif()

# optional for LLM (local LLM inference via llama.cpp with CUDA)
if(CVEDIX_WITH_LLM)

    # LLM local inference with llama.cpp can optionally use CUDA.
    if(CVEDIX_LLAMACPP_WITH_CUDA AND NOT CVEDIX_WITH_CUDA)
        message(FATAL_ERROR "\n"
            "========================================\n"
            "CVEDIX_WITH_LLM + CVEDIX_LLAMACPP_WITH_CUDA requires CUDA!\n"
            "========================================\n"
            "Local llama.cpp CUDA backend is enabled but CUDA is OFF.\n"
            "\n"
            "Enable CUDA:\n"
            "  cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_LLM=ON -DCVEDIX_LLAMACPP_WITH_CUDA=ON ..\n"
            "\n"
            "Or build llama.cpp in CPU mode:\n"
            "  cmake -DCVEDIX_WITH_LLM=ON -DCVEDIX_LLAMACPP_WITH_CUDA=OFF ..\n"
            "\n"
            "Or disable LLM:\n"
            "  cmake -DCVEDIX_WITH_LLM=OFF ..\n"
            "========================================\n"
        )
    endif()

    set(CVEDIX_CORE_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

    # Check that llama.cpp submodule exists
    if(NOT EXISTS "${CVEDIX_CORE_THIRD_PARTY_DIR}/llama.cpp/CMakeLists.txt")
        message(FATAL_ERROR "\n"
            "========================================\n"
            "llama.cpp submodule not found!\n"
            "========================================\n"
            "Please initialize the git submodule:\n"
            "  cd 3rdpart/CoreAI && git submodule update --init third_party/llama.cpp\n"
            "========================================\n"
        )
    endif()

    # Configure llama.cpp build options
    if(CVEDIX_LLAMACPP_WITH_CUDA)
        set(GGML_CUDA ON CACHE BOOL "" FORCE)
        set(GGML_CUDA_F16 ON CACHE BOOL "" FORCE)
        # Auto-detect GPU architecture if not explicitly set
        # Supports: sm_75 (Turing), sm_80 (A100), sm_86 (RTX 30xx), sm_87 (Jetson Orin),
        #           sm_89 (RTX 40xx), sm_90 (H100)
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
            set(CMAKE_CUDA_ARCHITECTURES "native" CACHE STRING "CUDA architectures" FORCE)
            message(STATUS "CUDA arch auto-detected: native")
        endif()
    else()
        set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    endif()
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)

    message("-------------start build llama.cpp--------------")
    add_subdirectory(third_party/llama.cpp)
    message("--------------end build llama.cpp---------------")

    # Build LLM engine wrapper
    message("-------------start build llm_engine---------")
    add_subdirectory(third_party/llm_engine)
    list(APPEND CVEDIX_PRIVATE_DEPEND_LIBS llm_engine)
    message("--------------end build llm_engine----------")

    message(STATUS "LLM library status:")
    message(STATUS "    llama.cpp: ${CVEDIX_CORE_THIRD_PARTY_DIR}/llama.cpp")
    message(STATUS "    llama.cpp CUDA: ${CVEDIX_LLAMACPP_WITH_CUDA}")
    message(STATUS "    CUDA arch: ${CMAKE_CUDA_ARCHITECTURES}")
    message(STATUS "    LLM engine: enabled")
endif()

# FFmpeg mandatory
list(APPEND CVEDIX_DEPEND_LIBS avcodec avformat avdevice swscale swresample avutil)

# MQTT mandatory (libmosquitto)
pkg_check_modules(MOSQUITTO mosquitto)
if(MOSQUITTO_FOUND)
    message(STATUS "MQTT (libmosquitto) found via pkg-config")
    message(STATUS "    version: ${MOSQUITTO_VERSION}")
    message(STATUS "    libraries: ${MOSQUITTO_LIBRARIES}")
    message(STATUS "    include path: ${MOSQUITTO_INCLUDE_DIRS}")
    include_directories(${MOSQUITTO_INCLUDE_DIRS})
    list(APPEND CVEDIX_DEPEND_LIBS ${MOSQUITTO_LIBRARIES})
else()
    find_library(MOSQUITTO_LIB
        NAMES mosquitto
        PATHS /usr/local/lib /usr/lib
        NO_DEFAULT_PATH
    )
    if(NOT MOSQUITTO_LIB)
        find_library(MOSQUITTO_LIB NAMES mosquitto)
    endif()
    if(NOT MOSQUITTO_LIB)
        message(FATAL_ERROR "\n"
            "========================================\n"
            "libmosquitto not found!\n"
            "========================================\n"
            "libmosquitto is required for MQTT support\n"
            "\n"
            "To install:\n"
            "  Ubuntu/Debian: sudo apt-get install libmosquitto-dev\n"
            "  Fedora/RHEL:   sudo dnf install libmosquitto-devel\n"
            "========================================\n"
        )
    endif()
    message(STATUS "MQTT (libmosquitto) library found: ${MOSQUITTO_LIB}")
    list(APPEND CVEDIX_DEPEND_LIBS ${MOSQUITTO_LIB})

    find_path(MOSQUITTO_INCLUDE_DIR
        NAMES mosquitto.h
        PATHS /usr/local/include /usr/include
        NO_DEFAULT_PATH
    )
    if(NOT MOSQUITTO_INCLUDE_DIR)
        find_path(MOSQUITTO_INCLUDE_DIR NAMES mosquitto.h)
    endif()
    if(MOSQUITTO_INCLUDE_DIR)
        message(STATUS "MQTT (libmosquitto) include path: ${MOSQUITTO_INCLUDE_DIR}")
        include_directories(${MOSQUITTO_INCLUDE_DIR})
    endif()
endif()
message(STATUS "MQTT support: enabled")

# optional for RKNN (Rockchip NPU)
if(CVEDIX_WITH_RKNN)
    add_definitions(-DCVEDIX_WITH_RKNN)
    
    # Check if RKNN is available in third_party/rknn
    set(RKNN_THIRD_PARTY_LIB_DIR ${CMAKE_SOURCE_DIR}/third_party/rknn/libs)
    set(RKNN_THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/rknn/include)
    set(RKNN_BUILD_LIB_DIR ${CMAKE_BINARY_DIR}/libs/rknn)
    set(RKNN_BUILD_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include/rknn)
    
    # Check if third_party/rknn exists
    if(EXISTS ${RKNN_THIRD_PARTY_LIB_DIR} AND EXISTS ${RKNN_THIRD_PARTY_INCLUDE_DIR})
        message(STATUS "RKNN found in third_party/rknn - will copy to build directory")
        
        # Create build directories
        file(MAKE_DIRECTORY ${RKNN_BUILD_LIB_DIR})
        file(MAKE_DIRECTORY ${RKNN_BUILD_INCLUDE_DIR})
        
        # Get list of files to copy
        file(GLOB RKNN_LIB_FILES "${RKNN_THIRD_PARTY_LIB_DIR}/*.so*")
        file(GLOB RKNN_HEADER_FILES "${RKNN_THIRD_PARTY_INCLUDE_DIR}/*.h")
        
        # Create custom command to copy libraries
        set(RKNN_COPIED_LIBS "")
        foreach(lib_file ${RKNN_LIB_FILES})
            get_filename_component(lib_name ${lib_file} NAME)
            set(dest_lib ${RKNN_BUILD_LIB_DIR}/${lib_name})
            add_custom_command(
                OUTPUT ${dest_lib}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${lib_file} ${dest_lib}
                DEPENDS ${lib_file}
                COMMENT "Copying RKNN library: ${lib_name}"
            )
            list(APPEND RKNN_COPIED_LIBS ${dest_lib})
        endforeach()
        
        # Create custom command to copy headers
        set(RKNN_COPIED_HEADERS "")
        foreach(header_file ${RKNN_HEADER_FILES})
            get_filename_component(header_name ${header_file} NAME)
            set(dest_header ${RKNN_BUILD_INCLUDE_DIR}/${header_name})
            add_custom_command(
                OUTPUT ${dest_header}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${header_file} ${dest_header}
                DEPENDS ${header_file}
                COMMENT "Copying RKNN header: ${header_name}"
            )
            list(APPEND RKNN_COPIED_HEADERS ${dest_header})
        endforeach()
        
        # Create custom target to ensure files are copied
        add_custom_target(copy_rknn_files
            DEPENDS ${RKNN_COPIED_LIBS} ${RKNN_COPIED_HEADERS}
            COMMENT "Copying RKNN libraries and headers to build directory"
        )
        
        message(STATUS "  RKNN libraries will be copied to: ${RKNN_BUILD_LIB_DIR}")
        message(STATUS "  RKNN headers will be copied to: ${RKNN_BUILD_INCLUDE_DIR}")
        
        # Use copied files - set library path directly
        set(RKNN_LIB_DIR ${RKNN_BUILD_LIB_DIR})
        set(RKNN_INCLUDE_DIR ${RKNN_BUILD_INCLUDE_DIR})
        
        # Check if library exists in source directory
        if(EXISTS ${RKNN_THIRD_PARTY_LIB_DIR}/librknnrt.so)
            # Add link directory and use library name (CMake will find it at link time)
            link_directories(${RKNN_BUILD_LIB_DIR})
            list(APPEND CVEDIX_DEPEND_LIBS rknnrt)
            message(STATUS "RKNN library (rknnrt) will be linked from: ${RKNN_BUILD_LIB_DIR}")
        else()
            message(WARNING "RKNN library (librknnrt.so) not found in ${RKNN_THIRD_PARTY_LIB_DIR}")
        endif()
        
        include_directories(${RKNN_BUILD_INCLUDE_DIR})
        message(STATUS "RKNN include path: ${RKNN_BUILD_INCLUDE_DIR}")
        message(STATUS "RKNN library path: ${RKNN_BUILD_LIB_DIR}")
        
        # Install libraries and headers
        install(DIRECTORY ${RKNN_BUILD_LIB_DIR}/
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/rknn
            FILES_MATCHING PATTERN "*.so*"
        )
        install(DIRECTORY ${RKNN_BUILD_INCLUDE_DIR}/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rknn
            FILES_MATCHING PATTERN "*.h"
        )
    else()
        # Fallback: Find librknnrt.so - typically installed in /usr/local/lib or /usr/lib
        find_library(RKNN_LIB 
            NAMES rknnrt
            PATHS /usr/local/lib /usr/lib /opt/rockchip/lib
            NO_DEFAULT_PATH
        )
        if(RKNN_LIB)
            message(STATUS "RKNN library found: ${RKNN_LIB}")
            list(APPEND CVEDIX_DEPEND_LIBS ${RKNN_LIB})
        else()
            # Try to find in standard paths
            find_library(RKNN_LIB NAMES rknnrt)
            if(RKNN_LIB)
                message(STATUS "RKNN library found: ${RKNN_LIB}")
                list(APPEND CVEDIX_DEPEND_LIBS ${RKNN_LIB})
            else()
                message(WARNING "RKNN library (librknnrt.so) not found. Please install rknn-toolkit2.")
            endif()
        endif()
        
        # Find rknn_api.h - typically in /usr/local/include or /usr/include
        find_path(RKNN_INCLUDE_DIR
            NAMES rknn_api.h
            PATHS /usr/local/include /usr/include /opt/rockchip/include
            NO_DEFAULT_PATH
        )
        if(RKNN_INCLUDE_DIR)
            message(STATUS "RKNN include path: ${RKNN_INCLUDE_DIR}")
            include_directories(${RKNN_INCLUDE_DIR})
        else()
            find_path(RKNN_INCLUDE_DIR NAMES rknn_api.h)
            if(RKNN_INCLUDE_DIR)
                message(STATUS "RKNN include path: ${RKNN_INCLUDE_DIR}")
                include_directories(${RKNN_INCLUDE_DIR})
            else()
                message(WARNING "RKNN headers (rknn_api.h) not found. Please install rknn-toolkit2.")
            endif()
        endif()
    endif()
    message(STATUS "RKNN support: enabled")
     # Pass RGA option to rknn_yolov11 subdirectory
    if(CVEDIX_WITH_RGA)
        set(CVEDIX_WITH_RGA ON CACHE INTERNAL "Pass RGA option to subdirectories")
    endif()
    
    message("--------------start build rknn_yolov11--------------")
    add_subdirectory(third_party/rknn_yolov11)
    message("--------------end build rknn_yolov11---------------")

    message("--------------start build rknn_yolov26--------------")
    add_subdirectory(third_party/rknn_yolov26)
    message("--------------end build rknn_yolov26---------------")
    
    message(STATUS "RKNN support: enabled")
endif()


# optional for RGA (Rockchip Graphics Accelerator)
if(CVEDIX_WITH_RGA)
    add_definitions(-DCVEDIX_WITH_RGA)
    
    # Check if RGA is available in third_party/librga
    set(RGA_THIRD_PARTY_LIB_DIR ${CMAKE_SOURCE_DIR}/third_party/librga/libs)
    set(RGA_THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/librga/include)
    set(RGA_BUILD_LIB_DIR ${CMAKE_BINARY_DIR}/libs/rga)
    set(RGA_BUILD_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include/rga)
    
    # Check if third_party/librga exists
    if(EXISTS ${RGA_THIRD_PARTY_LIB_DIR} AND EXISTS ${RGA_THIRD_PARTY_INCLUDE_DIR})
        message(STATUS "RGA found in third_party/librga - will copy to build directory")
        
        # Create build directories
        file(MAKE_DIRECTORY ${RGA_BUILD_LIB_DIR})
        file(MAKE_DIRECTORY ${RGA_BUILD_INCLUDE_DIR})
        
        # Get list of files to copy
        file(GLOB RGA_LIB_FILES "${RGA_THIRD_PARTY_LIB_DIR}/*.so*" "${RGA_THIRD_PARTY_LIB_DIR}/*.a")
        file(GLOB RGA_HEADER_FILES "${RGA_THIRD_PARTY_INCLUDE_DIR}/*.h" "${RGA_THIRD_PARTY_INCLUDE_DIR}/*.hpp")
        
        # Create custom command to copy libraries
        set(RGA_COPIED_LIBS "")
        foreach(lib_file ${RGA_LIB_FILES})
            get_filename_component(lib_name ${lib_file} NAME)
            set(dest_lib ${RGA_BUILD_LIB_DIR}/${lib_name})
            add_custom_command(
                OUTPUT ${dest_lib}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${lib_file} ${dest_lib}
                DEPENDS ${lib_file}
                COMMENT "Copying RGA library: ${lib_name}"
            )
            list(APPEND RGA_COPIED_LIBS ${dest_lib})
        endforeach()
        
        # Create custom command to copy headers
        set(RGA_COPIED_HEADERS "")
        foreach(header_file ${RGA_HEADER_FILES})
            get_filename_component(header_name ${header_file} NAME)
            set(dest_header ${RGA_BUILD_INCLUDE_DIR}/${header_name})
            add_custom_command(
                OUTPUT ${dest_header}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${header_file} ${dest_header}
                DEPENDS ${header_file}
                COMMENT "Copying RGA header: ${header_name}"
            )
            list(APPEND RGA_COPIED_HEADERS ${dest_header})
        endforeach()
        
        # Create custom target to ensure files are copied
        add_custom_target(copy_rga_files
            DEPENDS ${RGA_COPIED_LIBS} ${RGA_COPIED_HEADERS}
            COMMENT "Copying RGA libraries and headers to build directory"
        )
        
        message(STATUS "  RGA libraries will be copied to: ${RGA_BUILD_LIB_DIR}")
        message(STATUS "  RGA headers will be copied to: ${RGA_BUILD_INCLUDE_DIR}")
        
        # Use copied files - set library path directly
        set(RGA_LIB_DIR ${RGA_BUILD_LIB_DIR})
        set(RGA_INCLUDE_DIR ${RGA_BUILD_INCLUDE_DIR})
        
        # Check if library exists in source directory (prefer .so over .a)
        if(EXISTS ${RGA_THIRD_PARTY_LIB_DIR}/librga.so)
            # Add link directory and use library name (CMake will find it at link time)
            link_directories(${RGA_BUILD_LIB_DIR})
            list(APPEND CVEDIX_DEPEND_LIBS rga)
            message(STATUS "RGA library (rga) will be linked from: ${RGA_BUILD_LIB_DIR}")
        elseif(EXISTS ${RGA_THIRD_PARTY_LIB_DIR}/librga.a)
            # Use static library
            link_directories(${RGA_BUILD_LIB_DIR})
            list(APPEND CVEDIX_DEPEND_LIBS rga)
            message(STATUS "RGA static library (rga) will be linked from: ${RGA_BUILD_LIB_DIR}")
        else()
            message(WARNING "RGA library (librga.so or librga.a) not found in ${RGA_THIRD_PARTY_LIB_DIR}")
        endif()
        
        include_directories(${RGA_BUILD_INCLUDE_DIR})
        message(STATUS "RGA include path: ${RGA_BUILD_INCLUDE_DIR}")
        message(STATUS "RGA library path: ${RGA_BUILD_LIB_DIR}")
        
        # Install libraries and headers
        install(DIRECTORY ${RGA_BUILD_LIB_DIR}/
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/rga
            FILES_MATCHING PATTERN "*.so*" PATTERN "*.a"
        )
        install(DIRECTORY ${RGA_BUILD_INCLUDE_DIR}/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rga
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
        )
    else()
        # Fallback: Find librga.so
        find_library(RGA_LIB
            NAMES rga
            PATHS /usr/local/lib /usr/lib /opt/rockchip/lib
            NO_DEFAULT_PATH
        )
        if(RGA_LIB)
            message(STATUS "RGA library found: ${RGA_LIB}")
            list(APPEND CVEDIX_DEPEND_LIBS ${RGA_LIB})
        else()
            find_library(RGA_LIB NAMES rga)
            if(RGA_LIB)
                message(STATUS "RGA library found: ${RGA_LIB}")
                list(APPEND CVEDIX_DEPEND_LIBS ${RGA_LIB})
            else()
                message(WARNING "RGA library (librga.so) not found. Please install RGA library.")
            endif()
        endif()
        
        # Find rga.h
        find_path(RGA_INCLUDE_DIR
            NAMES rga.h
            PATHS /usr/local/include /usr/include /opt/rockchip/include
            NO_DEFAULT_PATH
        )
        if(RGA_INCLUDE_DIR)
            message(STATUS "RGA include path: ${RGA_INCLUDE_DIR}")
            include_directories(${RGA_INCLUDE_DIR})
        else()
            find_path(RGA_INCLUDE_DIR NAMES rga.h)
            if(RGA_INCLUDE_DIR)
                message(STATUS "RGA include path: ${RGA_INCLUDE_DIR}")
                include_directories(${RGA_INCLUDE_DIR})
            else()
                message(WARNING "RGA headers (rga.h) not found. Please install RGA library.")
            endif()
        endif()
    endif()
    message(STATUS "RGA support: enabled")
endif()

# optional for ONNX Runtime (for ORT-based nodes)
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH "ONNX Runtime root")
if(EXISTS ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)
    add_definitions(-DCVEDIX_WITH_ORT)
    include_directories(${ONNXRUNTIME_DIR}/include)
    link_directories(${ONNXRUNTIME_DIR}/lib)
    list(APPEND CVEDIX_DEPEND_LIBS onnxruntime)
    message(STATUS "ONNX Runtime found at ${ONNXRUNTIME_DIR} - ORT nodes will be available")
    set(CVEDIX_WITH_ORT ON)
else()
    message(STATUS "ONNX Runtime not found at ${ONNXRUNTIME_DIR} - ORT nodes disabled")
    set(CVEDIX_WITH_ORT OFF)
endif()

# ── CVEDIX YOLO ORT Detector Plugin (ONNX Runtime) ──
set(CVEDIX_YOLO_ORT_DETECTOR_DIR ${CMAKE_SOURCE_DIR}/third_party/cvedix_yolo_ort_detector)
if(CVEDIX_WITH_ORT AND EXISTS ${CVEDIX_YOLO_ORT_DETECTOR_DIR}/CMakeLists.txt)
    add_subdirectory(${CVEDIX_YOLO_ORT_DETECTOR_DIR})
    list(APPEND CVEDIX_DEPEND_LIBS cvedix_yolo_ort_detector)
    message(STATUS "CVEDIX YOLO ORT Detector plugin enabled")
endif()

# optional for SeetaFace6 (face detection, recognition, landmarks, etc.)
set(SEETAFACE6_DIR ${CMAKE_SOURCE_DIR}/third_party/seetaface6)
set(SEETAFACE6_BUILD_DIR ${SEETAFACE6_DIR}/build)
if(EXISTS ${SEETAFACE6_BUILD_DIR}/lib AND EXISTS ${SEETAFACE6_BUILD_DIR}/include)
    add_definitions(-DCVEDIX_WITH_FACE)
    include_directories(${SEETAFACE6_BUILD_DIR}/include)
    link_directories(${SEETAFACE6_BUILD_DIR}/lib)
    list(APPEND CVEDIX_DEPEND_LIBS
        SeetaFaceDetector600
        SeetaFaceLandmarker600
        SeetaFaceRecognizer610
        SeetaMaskDetector200
        SeetaAgePredictor600
        SeetaGenderPredictor600
        SeetaEyeStateDetector200
        SeetaPoseEstimation600
        SeetaFaceAntiSpoofingX600
        tennis
    )
    set(CVEDIX_WITH_FACE ON)
    message(STATUS "SeetaFace6 found at ${SEETAFACE6_BUILD_DIR} - SeetaFace nodes will be available")
else()
    message(STATUS "SeetaFace6 not built at ${SEETAFACE6_BUILD_DIR} - SeetaFace nodes disabled")
    message(STATUS "  To build: cd ${SEETAFACE6_DIR} && bash build_seetaface6.sh")
    set(CVEDIX_WITH_FACE OFF)
endif()

# optional for Milvus vector database (large-scale face search)
set(MILVUS_SDK_DIR ${CMAKE_SOURCE_DIR}/third_party)
set(MILVUS_SDK_BUILD_DIR ${MILVUS_SDK_DIR}/milvus-sdk-cpp-build)
if(CVEDIX_WITH_MILVUS)
    if(EXISTS ${MILVUS_SDK_BUILD_DIR}/lib AND EXISTS ${MILVUS_SDK_BUILD_DIR}/include)
        add_definitions(-DCVEDIX_WITH_MILVUS)
        include_directories(${MILVUS_SDK_BUILD_DIR}/include)
        # Add third_party to include path so that <nlohmann/json.hpp> can be resolved
        include_directories(${CMAKE_SOURCE_DIR}/third_party)
        link_directories(${MILVUS_SDK_BUILD_DIR}/lib)
        list(APPEND CVEDIX_DEPEND_LIBS milvus_sdk)
        message(STATUS "Milvus SDK found at ${MILVUS_SDK_BUILD_DIR} - Milvus face search node will be available")
    else()
        message(WARNING "Milvus SDK not built at ${MILVUS_SDK_BUILD_DIR} - Milvus nodes disabled")
        message(STATUS "  To build: cd ${MILVUS_SDK_DIR} && bash build_milvus_sdk.sh")
        set(CVEDIX_WITH_MILVUS OFF)
    endif()
else()
    message(STATUS "Milvus support: disabled (use -DCVEDIX_WITH_MILVUS=ON to enable)")
endif()

# tinyexpr
message("-------------start build tinyexpr--------------")
add_subdirectory(third_party/tinyexpr)
# list(APPEND CVEDIX_DEPEND_LIBS tinyexpr) # Linked PRIVATE later
install(TARGETS tinyexpr EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
message("--------------end build tinyexpr---------------")

message("-----------------include asio------------------")
add_subdirectory(third_party/asio)
list(APPEND CVEDIX_DEPEND_LIBS asio) 
install(TARGETS asio EXPORT cvedix-targets)

message("-------------collect version info--------------")
string(TIMESTAMP BUILD_TIME "%Y%m%d-%H%M%S")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIFF_RESULT
    )
    if(GIT_DIFF_RESULT EQUAL 0)
        set(GIT_DIRTY "")
    else()
        set(GIT_DIRTY "-dirty")
    endif()
else()
    message(WARNING "Git not found! Using default version info.")
    set(GIT_COMMIT_HASH "nogit")
    set(GIT_DIRTY "")
endif()
set(FINAL_GIT_VERSION "${GIT_COMMIT_HASH}${GIT_DIRTY}")
configure_file(
    ${CMAKE_SOURCE_DIR}/utils/cvedix_version.h.in
    ${CMAKE_BINARY_DIR}/cvedix_version.h
    @ONLY
)
include_directories(${CMAKE_BINARY_DIR})
message(STATUS "version info:")
message(STATUS "    build_time: ${BUILD_TIME}")
message(STATUS "    commit_hash: ${FINAL_GIT_VERSION}")
message("-------------collect version info--------------")

# collect source files for SDK
file(GLOB_RECURSE NODES "nodes/*.cpp")
file(GLOB_RECURSE OBJECTS "objects/*.cpp")
file(GLOB_RECURSE UTILS "utils/*.cpp")

# --- FILTER SOURCES FOR CORE LIBRARY ---
set(CORE_NODES ${NODES})
set(CORE_UTILS ${UTILS})

# Always exclude Hardware implementation from Core
list(FILTER CORE_NODES EXCLUDE REGEX ".*rknn.*")
# Exclude removed trt_vehicle nodes and other disabled TRT modules
if(NOT CVEDIX_WITH_TRT)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*trt.*")
else()
    # Exclude removed/legacy TRT nodes
    list(FILTER CORE_NODES EXCLUDE REGEX ".*trt_vehicle.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*trt_insight.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*trt_yolov8.*")  # Legacy disabled trt_yolov8 lib
    list(FILTER CORE_NODES EXCLUDE REGEX ".*face_recognition_trt.*")  # Depends on missing insightface
    # Exclude face_recognition_node if trt_insightface headers are missing
    # (tensorrt_backend.h includes trt_insightface which may not be built)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/trt_insightface/models/insight_face_recognition.h")
        list(FILTER CORE_NODES EXCLUDE REGEX ".*fr/cvedix_face_recognition_node.*")
        message(STATUS "trt_insightface headers not found - face_recognition_node excluded from TRT build")
    endif()
endif()
list(FILTER CORE_UTILS EXCLUDE REGEX ".*rknn.*")
list(FILTER CORE_UTILS EXCLUDE REGEX ".*rga.*")


# Exclude LLM-related files if LLM is not enabled
if(NOT CVEDIX_WITH_LLM)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*mllm.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*llm.*")
endif()

# Exclude Paddle-related nodes if Paddle is not enabled
if(NOT CVEDIX_WITH_PADDLE)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*paddle.*")
endif()

# (RKNN/TRT exclusions handled by default above for Core)



# Exclude license-related files (license feature removed)
list(FILTER CORE_UTILS EXCLUDE REGEX ".*license.*")

# Exclude ONNX Runtime-based nodes if ORT is not available
if(NOT CVEDIX_WITH_ORT)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*_ort_node.*")
endif()

# Exclude SeetaFace-based nodes if SeetaFace6 is not built
if(NOT CVEDIX_WITH_FACE)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*face_recognizer_node.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*face_analysis_node.*")
endif()

# Exclude Milvus-based nodes if Milvus SDK is not available
if(NOT CVEDIX_WITH_MILVUS)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*milvus.*")
endif()

# Add PaddleDetection core sources when Paddle is enabled
if(CVEDIX_WITH_PADDLE)
    list(APPEND CORE_UTILS ${CVEDIX_PADDLE_DETECTION_SRCS})
endif()

# Add EIGEN
set(EIGEN_VERSION 3.3)
find_package(Eigen3 ${EIGEN_VERSION} QUIET)
message("FIND Eigen3")
list(APPEND CVEDIX_DEPEND_LIBS Eigen3::Eigen)
if(NOT EIGEN3_FOUND)
    message("NOT FOUND EIGEN3")
    set(BUILD_TESTING OFF CACHE INTERNAL "")
    FetchContent_Declare(eigen
        GIT_REPOSITORY  https://gitlab.com/libeigen/eigen.git
        GIT_TAG         ${EIGEN_VERSION}
        GIT_SHALLOW     ON)
    FetchContent_MakeAvailable(eigen)
    unset(BUILD_TESTING CACHE)
endif()

# build for CORE SDK
get_filename_component(RAPIDMEDIA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MEDIATOOLKIT_LIB_SEARCH_PATHS
    "${CMAKE_BINARY_DIR}/3rdpart/MediaToolKit/lib"
    "${RAPIDMEDIA_ROOT}/build/3rdpart/MediaToolKit/lib"
    "${RAPIDMEDIA_ROOT}/build-debug/3rdpart/MediaToolKit/lib"
    "${RAPIDMEDIA_ROOT}/build-release/3rdpart/MediaToolKit/lib"
)

find_library(MEDIATOOLKIT_LIB
    NAMES MediaToolKit
    PATHS ${MEDIATOOLKIT_LIB_SEARCH_PATHS}
)

if(NOT MEDIATOOLKIT_LIB)
    message(FATAL_ERROR
        "MediaToolKit library not found. Checked: ${MEDIATOOLKIT_LIB_SEARCH_PATHS}. "
        "Build MediaToolKit first or provide library path in one of these locations.")
endif()

add_library(cvedix_core SHARED ${CORE_NODES} ${CORE_UTILS} ${OBJECTS})
target_link_libraries(cvedix_core
    PUBLIC
        ${CVEDIX_DEPEND_LIBS}
        dl
    PRIVATE
        tinyexpr
        ${MEDIATOOLKIT_LIB}
        ${CVEDIX_PRIVATE_DEPEND_LIBS}
)

# ---------------------------------------------------------------------------
# ABI/ODR safety: layout-affecting feature macros MUST be visible to every
# consumer that includes cvedix headers, not just to cvedix_core's own sources.
#
# CVEDIX_WITH_KAFKA adds a `cvedix_log_kafka_writer kafka_writer` member to the
# public `cvedix_logger` class. If the library is compiled with this macro but a
# consumer (e.g. rapidmedia_ai, server) is not, the two disagree on the class
# layout: the consumer's CVEDIX_SET_LOG_* writes land at the wrong offsets and
# corrupt the logger singleton's members (observed as a SIGSEGV inside
# cvedix_logger::log iterating a corrupted keywords_for_debug_log vector).
#
# add_definitions() is directory-scoped and does NOT propagate to consumer
# targets in sibling directories, so we export the macro (and the librdkafka
# include dir it pulls in) as PUBLIC usage requirements of cvedix_core.
# ---------------------------------------------------------------------------
target_compile_definitions(cvedix_core PUBLIC CVEDIX_WITH_KAFKA)
target_include_directories(cvedix_core PUBLIC $<BUILD_INTERFACE:${RDKAFKA_INCLUDE_DIR}>)

# Alias
add_library(${PROJECT_NAME} ALIAS cvedix_core)

# Create a cvedix directory structure in build directory for build-time includes
# This allows headers to use "cvedix/..." paths both when building from source and when installed
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cvedix)
# Create symlinks for nodes, objects, utils, excepts in build directory
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/cvedix/nodes
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/nodes ${CMAKE_BINARY_DIR}/cvedix/nodes
    COMMENT "Creating cvedix/nodes symlink"
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/cvedix/objects
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/objects ${CMAKE_BINARY_DIR}/cvedix/objects
    COMMENT "Creating cvedix/objects symlink"
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/cvedix/utils
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/utils ${CMAKE_BINARY_DIR}/cvedix/utils
    COMMENT "Creating cvedix/utils symlink"
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/cvedix/excepts
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/excepts ${CMAKE_BINARY_DIR}/cvedix/excepts
    COMMENT "Creating cvedix/excepts symlink"
)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/cvedix/third_party
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/third_party ${CMAKE_BINARY_DIR}/cvedix/third_party
    COMMENT "Creating cvedix/third_party symlink"
)
# Create a custom target to ensure symlinks are created before building
add_custom_target(cvedix_symlinks
    DEPENDS ${CMAKE_BINARY_DIR}/cvedix/nodes
            ${CMAKE_BINARY_DIR}/cvedix/objects
            ${CMAKE_BINARY_DIR}/cvedix/utils
            ${CMAKE_BINARY_DIR}/cvedix/excepts
            ${CMAKE_BINARY_DIR}/cvedix/third_party
)

target_include_directories(cvedix_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Ensure symlinks are created before building the library
add_dependencies(cvedix_core cvedix_symlinks)

# Ensure RKNN and RGA files are copied before building (if enabled)
if(CVEDIX_WITH_RKNN)
    if(TARGET copy_rknn_files)
        add_dependencies(cvedix_core copy_rknn_files)
    endif()
endif()
if(CVEDIX_WITH_RGA)
    if(TARGET copy_rga_files)
        add_dependencies(cvedix_core copy_rga_files)
    endif()
endif()

# Install CVEDIX YOLO ORT Detector plugin (required by cvedix_core EXPORT)
if(CVEDIX_WITH_ORT AND TARGET cvedix_yolo_ort_detector)
    install(TARGETS cvedix_yolo_ort_detector EXPORT cvedix-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix
    )
endif()



# Installation
install(TARGETS cvedix_core EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix
)

install(DIRECTORY nodes objects utils excepts
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix
    FILES_MATCHING PATTERN "*.h"
)

# Install third_party headers so cvedix/third_party/... paths work after installation
install(DIRECTORY third_party
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix
    FILES_MATCHING 
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.ipp"
    PATTERN ".git" EXCLUDE
    PATTERN "*.md" EXCLUDE
)

install(FILES ${CMAKE_BINARY_DIR}/cvedix_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix
)

# Export targets
install(EXPORT cvedix-targets
    FILE cvedix-targets.cmake
    NAMESPACE cvedix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix/cmake
)

# Config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Collect compile definitions that were used when building the SDK
# Note: GStreamer, FFmpeg, MQTT, Kafka are mandatory and always defined via add_definitions()
# Only optional features need to be propagated to consumer projects
set(CVEDIX_CONFIG_DEFINES "")
if(CVEDIX_WITH_RKNN)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_RKNN)\n")
endif()
if(CVEDIX_WITH_RGA)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_RGA)\n")
endif()
if(CVEDIX_WITH_CUDA)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_CUDA)\n")
endif()
if(CVEDIX_WITH_TRT)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_TRT)\n")
endif()
if(CVEDIX_WITH_PADDLE)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_PADDLE)\n")
endif()
if(CVEDIX_WITH_LLM)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_LLM)\n")
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cvedix-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix/cmake
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix/cmake
)

# Install pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cvedix.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix.pc"
    @ONLY
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cvedix.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix/pkgconfig
)

# build for samples (optional - disabled by default to save build time and resources)
if(CVEDIX_BUILD_SAMPLES)
    message(STATUS "Building samples (this may take longer)")
    add_subdirectory(samples)
else()
    message(STATUS "Skipping samples build (enable with -DCVEDIX_BUILD_SAMPLES=ON)")
endif()

# C API layer for language bindings (Java/JNA, C#/P-Invoke, C)
if(CVEDIX_BUILD_CAPI)
    message(STATUS "Building C API layer (libcvedix_capi)")
    add_subdirectory(capi)
else()
    message(STATUS "Skipping C API build (enable with -DCVEDIX_BUILD_CAPI=ON)")
endif()

# Unit + smoke tests. Default OFF so existing builds are unaffected; they add no
# new dependency (assertions are hand-rolled) and need no models or GPU.
#   cmake -DCVEDIX_BUILD_TESTS=ON .. && cmake --build . && ctest --output-on-failure
if(CVEDIX_BUILD_TESTS)
    message(STATUS "Building tests (run with: ctest --output-on-failure)")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Skipping tests build (enable with -DCVEDIX_BUILD_TESTS=ON)")
endif()


# build for benchmarks
# add_subdirectory(benchmarks)


# --- BẮT ĐẦU CẤU HÌNH ĐÓNG GÓI (PACKAGING) ---

# 1. Cài đặt thư mục data (models, config...) vào cùng thư mục bin
# Để chương trình có thể gọi ./cvedix_data/...
if(EXISTS "${CMAKE_SOURCE_DIR}/cvedix_data")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/cvedix_data"
            DESTINATION bin
            FILES_MATCHING PATTERN "*"
            PATTERN ".git" EXCLUDE)
    message(STATUS "cvedix_data directory will be included in package")
else()
    message(WARNING "cvedix_data directory not found - will not be included in package")
endif()

# 2. Cài đặt release notes vào doc directory
if(EXISTS "${CMAKE_SOURCE_DIR}/doc/RELEASE_NOTES_${PROJECT_VERSION}.md")
    install(FILES "${CMAKE_SOURCE_DIR}/doc/RELEASE_NOTES_${PROJECT_VERSION}.md"
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            RENAME "RELEASE_NOTES.md")
    message(STATUS "Release notes will be included in package")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/RELEASE_NOTES_${PROJECT_VERSION}.md")
    install(FILES "${CMAKE_SOURCE_DIR}/RELEASE_NOTES_${PROJECT_VERSION}.md"
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            RENAME "RELEASE_NOTES.md")
    message(STATUS "Release notes will be included in package")
endif()

# 2. Cấu hình CPack để tạo file .deb
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "edgeos-sdk")
set(CPACK_PACKAGE_VENDOR "CVEDIX")

# Generate version from timestamp and git hash
string(TIMESTAMP PACKAGE_TIMESTAMP "%Y%m%d%H%M%S")
# Use fixed version for release package with -dev suffix for development builds
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-dev1")
# For development builds with timestamp, uncomment below:
# set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-${PACKAGE_TIMESTAMP}-${FINAL_GIT_VERSION}")

# Package metadata
set(CPACK_PACKAGE_CONTACT "CVEDIX Team <support@cvedix.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CVEDIX AI Runtime SDK with RKNN, MQTT and JSON Output Support")
set(CPACK_PACKAGE_DESCRIPTION "CVEDIX AI Runtime SDK provides a comprehensive framework for building AI video processing pipelines with support for RKNN, TensorRT, and other inference backends. This version (2025.0.1.3-dev1) includes: Face recognition with ONNX Runtime (ORT) node supporting glint360k_r100 model, License management node with hardware binding, face recognition improvements with confidence gap, Custom Data Transform Node for data customization, MQTT broker node for publishing detection results via MQTT protocol, enhanced JSON output with customizable structure, base64 encoded images, and flexible JSON transformation capabilities. Package includes libraries, headers, samples, and cvedix_data directory.")

# Debian package metadata
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CVEDIX Team <support@cvedix.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Convert architecture name for Debian package
# aarch64 -> arm64 (Debian uses arm64, not aarch64)
# x86_64 -> amd64 (Debian uses amd64, not x86_64)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    set(PACKAGE_ARCH_SUFFIX "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(PACKAGE_ARCH_SUFFIX "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    set(PACKAGE_ARCH_SUFFIX "i386")
else()
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
    set(PACKAGE_ARCH_SUFFIX ${CMAKE_SYSTEM_PROCESSOR})
endif()

# Set package file name with architecture
# Format: cvedix-ai-runtime-2025.0.1.2-arm64.deb or cvedix-ai-runtime-2025.0.1.2-x86_64.deb
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${PACKAGE_ARCH_SUFFIX}")

# Cài đặt tất cả vào /opt/edgeos-sdk để tránh xung đột hệ thống và giữ đường dẫn tương đối
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/edgeos-sdk")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/cmake/debian/postinst"
    "${CMAKE_SOURCE_DIR}/cmake/debian/postrm"
)

# Tự động tìm dependencies (libs)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Dependencies hệ thống bắt buộc
set(CPACK_DEBIAN_PACKAGE_DEPENDS 
    "build-essential, make, cmake, pkg-config, mosquitto, mosquitto-clients, unzip, libmosquitto-dev, libturbojpeg, libturbojpeg-dev, libgstreamer1.0-0, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-ugly, gstreamer1.0-libav, gstreamer1.0-tools, gstreamer1.0-x, gstreamer1.0-alsa, gstreamer1.0-gl, gstreamer1.0-gtk3, gstreamer1.0-qt5, gstreamer1.0-pulseaudio, libgstreamer1.0-dev, libgstreamer-plugins-base1.0-dev, python3-gst-1.0, libgstrtspserver-1.0-dev, gstreamer1.0-rtsp, libgtk-3-dev, libavcodec-dev, libavformat-dev, libavdevice-dev, libavutil-dev, libswscale-dev, libswresample-dev, libv4l-dev, libxvidcore-dev, libx264-dev, libjpeg-dev, libpng-dev, libtiff-dev, gfortran, openexr, libatlas-base-dev, python3-dev, python3-numpy, libeigen3-dev, librdkafka-dev, libssl-dev, libcurl4-openssl-dev, libjson-c-dev")

# Recommends (khuyến nghị nhưng không bắt buộc)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "gstreamer1.0-rtsp, gstreamer1.0-plugins-ugly")

include(CPack)
