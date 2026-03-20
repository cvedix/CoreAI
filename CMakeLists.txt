#[[
    prepare in advance：
    1. OpenCV >= 4.10
    2. Gstreamer >= 1.14.5
    3. CUDA, TensorRT, PADDLE and others for optional
]]

cmake_minimum_required(VERSION 3.10)
project(cvedix_instance_sdk VERSION 2025.0.1.3)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -w -fdiagnostics-color=always -pthread")
# save all libs(including third_party's) to 'libs'
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)

# optional for build, modify values when configure the project using 'cmake -DCVEDIX_WITH_CUDA=OFF ..'
option(CVEDIX_WITH_GSTREAMER "prepared GStreamer or not?" ON)
option(CVEDIX_WITH_CUDA "prepared CUDA or not?" OFF)
option(CVEDIX_WITH_TRT "prepared TensorRT or not?" OFF)
option(CVEDIX_WITH_OPENVINO "prepared OpenVINO or not?" OFF)
option(CVEDIX_WITH_PADDLE "prepared PaddlePaddle or not?" OFF)
option(CVEDIX_WITH_KAFKA "prepared Kafka or not?" OFF)
option(CVEDIX_WITH_LLM "prepared LLM or not?" OFF)
option(CVEDIX_WITH_FFMPEG "prepared FFMPEG or not?" OFF)
option(CVEDIX_WITH_RKNN "prepared RKNN (Rockchip NPU) or not?" OFF)
option(CVEDIX_WITH_RGA "prepared RGA (Rockchip Graphics Accelerator) or not?" OFF)
option(CVEDIX_WITH_LICENSE "Enable license checking using licensecxx" OFF)
option(CVEDIX_BUILD_SAMPLES "build samples or not? (saves build time and resources)" OFF)
option(CVEDIX_BUILD_COMPLEX_SAMPLES "build complex samples or not? (maybe source code not provided)" OFF)

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

# Gstreamer optional
if(CVEDIX_WITH_GSTREAMER)
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
    set(CVEDIX_WITH_GSTREAMER ON)
else()
    message(STATUS "GStreamer disabled - GStreamer nodes will not be available")
    set(GST_DEPEND_LIBS "")
endif()

# collect dependent libs for SDK
list(APPEND CVEDIX_DEPEND_LIBS ${OpenCV_LIBS} stdc++fs)
if(CVEDIX_WITH_GSTREAMER)
    list(APPEND CVEDIX_DEPEND_LIBS ${GST_DEPEND_LIBS})
endif()

# optional for CUDA
if(CVEDIX_WITH_CUDA) # CUDA enabled
    add_definitions(-DCVEDIX_WITH_CUDA)
endif()

# ---------------- IMPORTANT ---------------- 
# Build default plugin backend
# onnx by opencv this is default CPU
add_subdirectory(third_party/onnx_yolov11)

# Build for TensorRT Backend
if(CVEDIX_WITH_TRT)  # TensorRT enabled
    add_definitions(-DCVEDIX_WITH_TRT)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project
    set(CVEDIX_TRT_LIB_PATH "/usr/local/tensorRT/lib/")         # need set the path here
    set(CVEDIX_TRT_INC_PATH "/usr/local/tensorRT/include/")     # need set the path here
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
    
    # trt_yolov11_face
    message("-------------start build trt_yolov11_face--------------")
    # Need refactor soon
    add_subdirectory(third_party/trt_yolov11_face)
    list(APPEND CVEDIX_DEPEND_LIBS trt_yolov11_face)
    message("--------------end build trt_yolov11_face---------------")

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
endif()

# optional for PaddlePaddle
if(CVEDIX_WITH_PADDLE)  # PaddlePaddle enabled
    add_definitions(-DCVEDIX_WITH_PADDLE)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project
    set(VP_BUILD_FROM ON)      # set flag for paddle_ocr sub project
    set(CVEDIX_PADDLE_LIB_PATH "")
    set(CVEDIX_PADDLE_INC_PATH "")
    message("-------------start build paddle_ocr--------------")
    add_subdirectory(third_party/paddle_ocr) # build paddle_ocr
    list(APPEND CVEDIX_DEPEND_LIBS paddle_ocr)
    
    # Use variables set by sub-project
    if(DEFINED VP_PADDLE_LIB_PATH)
        set(CVEDIX_PADDLE_LIB_PATH ${VP_PADDLE_LIB_PATH})
    endif()
    if(DEFINED VP_PADDLE_INC_PATH)
        set(CVEDIX_PADDLE_INC_PATH ${VP_PADDLE_INC_PATH})
    endif()

    link_directories(${CVEDIX_PADDLE_LIB_PATH})
    include_directories(${CVEDIX_PADDLE_INC_PATH})

    # Add dependencies library paths (propagate from paddle_inference path)
    # Support both old CPU structure and new GPU v3.0 structure
    set(PADDLE_INF_DIR "${CMAKE_SOURCE_DIR}/third_party/paddle_inference")
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
    
    message(STATUS "PaddlePaddle library status:")
    message(STATUS "    include path: ${CVEDIX_PADDLE_INC_PATH}")
    message(STATUS "    library path: ${CVEDIX_PADDLE_LIB_PATH}")
    message("--------------end build paddle_ocr---------------")
endif()

# optional for Kafka
if(CVEDIX_WITH_KAFKA)
    # Check if librdkafka is available
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
            "librdkafka is required when CVEDIX_WITH_KAFKA=ON\n"
            "\n"
            "To install librdkafka development packages:\n"
            "  Ubuntu/Debian: sudo apt-get install librdkafka-dev\n"
            "  Fedora/RHEL:   sudo dnf install librdkafka-devel\n"
            "  Arch Linux:    sudo pacman -S librdkafka\n"
            "\n"
            "Or disable Kafka support:\n"
            "  cmake -DCVEDIX_WITH_KAFKA=OFF ..\n"
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
endif()

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
endif()

# optional for FFmpeg
if(CVEDIX_WITH_FFMPEG)
    add_definitions(-DCVEDIX_WITH_FFMPEG)
    list(APPEND CVEDIX_DEPEND_LIBS avcodec avformat avdevice swscale swresample avutil)
endif()

# optional for MQTT (libmosquitto)
option(CVEDIX_WITH_MQTT "prepared MQTT (libmosquitto) or not?" OFF)
if(CVEDIX_WITH_MQTT)
    add_definitions(-DCVEDIX_WITH_MQTT)
    # Try to find libmosquitto using pkg-config first
    pkg_check_modules(MOSQUITTO mosquitto)
    if(MOSQUITTO_FOUND)
        message(STATUS "MQTT (libmosquitto) found via pkg-config")
        message(STATUS "    version: ${MOSQUITTO_VERSION}")
        message(STATUS "    libraries: ${MOSQUITTO_LIBRARIES}")
        message(STATUS "    include path: ${MOSQUITTO_INCLUDE_DIRS}")
        include_directories(${MOSQUITTO_INCLUDE_DIRS})
        list(APPEND CVEDIX_DEPEND_LIBS ${MOSQUITTO_LIBRARIES})
    else()
        # Try to find manually
        find_library(MOSQUITTO_LIB
            NAMES mosquitto
            PATHS /usr/local/lib /usr/lib
            NO_DEFAULT_PATH
        )
        if(MOSQUITTO_LIB)
            message(STATUS "MQTT (libmosquitto) library found: ${MOSQUITTO_LIB}")
            list(APPEND CVEDIX_DEPEND_LIBS ${MOSQUITTO_LIB})
        else()
            find_library(MOSQUITTO_LIB NAMES mosquitto)
            if(MOSQUITTO_LIB)
                message(STATUS "MQTT (libmosquitto) library found: ${MOSQUITTO_LIB}")
                list(APPEND CVEDIX_DEPEND_LIBS ${MOSQUITTO_LIB})
            else()
                message(WARNING "MQTT (libmosquitto) library not found. MQTT features will be disabled.")
                message(WARNING "  Install with: sudo apt-get install libmosquitto-dev")
                set(CVEDIX_WITH_MQTT OFF)
            endif()
        endif()
        
        # Find mosquitto.h
        find_path(MOSQUITTO_INCLUDE_DIR
            NAMES mosquitto.h
            PATHS /usr/local/include /usr/include
            NO_DEFAULT_PATH
        )
        if(MOSQUITTO_INCLUDE_DIR)
            message(STATUS "MQTT (libmosquitto) include path: ${MOSQUITTO_INCLUDE_DIR}")
            include_directories(${MOSQUITTO_INCLUDE_DIR})
        else()
            find_path(MOSQUITTO_INCLUDE_DIR NAMES mosquitto.h)
            if(MOSQUITTO_INCLUDE_DIR)
                message(STATUS "MQTT (libmosquitto) include path: ${MOSQUITTO_INCLUDE_DIR}")
                include_directories(${MOSQUITTO_INCLUDE_DIR})
            else()
                message(WARNING "MQTT (libmosquitto) headers not found. MQTT features will be disabled.")
                set(CVEDIX_WITH_MQTT OFF)
            endif()
        endif()
    endif()
    if(CVEDIX_WITH_MQTT)
        message(STATUS "MQTT support: enabled")
    endif()
endif()

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
            PATHS ${CMAKE_SOURCE_DIR}/third_party/rknn/libs /usr/local/lib /usr/lib /opt/rockchip/lib
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
            PATHS ${CMAKE_SOURCE_DIR}/third_party/rknn/include /usr/local/include /usr/include /opt/rockchip/include
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
endif()

# optional for License (licensecxx)
if(CVEDIX_WITH_LICENSE)
    add_definitions(-DCVEDIX_WITH_LICENSE)
    
    # OpenSSL is required by licensecxx (version >= 3.0 recommended)
    find_package(OpenSSL QUIET)
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR "\n"
            "========================================\n"
            "OpenSSL not found!\n"
            "========================================\n"
            "licensecxx requires OpenSSL (>= 3.0 recommended)\n"
            "\n"
            "To install OpenSSL development packages:\n"
            "  Ubuntu/Debian: sudo apt-get install libssl-dev\n"
            "  Fedora/RHEL:   sudo dnf install openssl-devel\n"
            "  Arch Linux:    sudo pacman -S openssl\n"
            "\n"
            "Or disable license checking:\n"
            "  cmake -DCVEDIX_WITH_LICENSE=OFF ..\n"
            "========================================\n"
        )
    endif()
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    
    # Check if licensecxx is available as submodule
    set(LICENSECXX_SUBMODULE_DIR ${CMAKE_SOURCE_DIR}/third_party/licensecxx)
    
    if(EXISTS ${LICENSECXX_SUBMODULE_DIR}/CMakeLists.txt)
        message(STATUS "licensecxx found in third_party/licensecxx - building as submodule")
        add_subdirectory(${LICENSECXX_SUBMODULE_DIR})
        
        # Link with licensecxx target
        if(TARGET lcxx::lcxx)
            message(STATUS "licensecxx target found: lcxx::lcxx")
            # Note: licensecxx will be linked via target_link_libraries in utils/CMakeLists.txt
        else()
            message(WARNING "licensecxx target lcxx::lcxx not found after adding subdirectory")
        endif()
    else()
        message(FATAL_ERROR "\n"
            "========================================\n"
            "licensecxx not found!\n"
            "========================================\n"
            "licensecxx is required when CVEDIX_WITH_LICENSE=ON\n"
            "\n"
            "To add licensecxx as submodule:\n"
            "  git submodule add https://github.com/felixjulianheitmann/licensecxx.git third_party/licensecxx\n"
            "  git submodule update --init --recursive\n"
            "\n"
            "Or disable license checking:\n"
            "  cmake -DCVEDIX_WITH_LICENSE=OFF ..\n"
            "========================================\n"
        )
    endif()
    
    message(STATUS "License checking support: enabled (licensecxx)")
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

# Exclude MQTT client and receiver if MQTT is not enabled
if(NOT CVEDIX_WITH_MQTT)
    list(FILTER CORE_UTILS EXCLUDE REGEX ".*mqtt_client.*")
    list(FILTER CORE_UTILS EXCLUDE REGEX ".*mqtt_json_receiver.*")
endif()

# Exclude Kafka-related files if Kafka is not enabled
if(NOT CVEDIX_WITH_KAFKA)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*kafka.*")
    list(FILTER CORE_UTILS EXCLUDE REGEX ".*kafka.*")
endif()

# Exclude LLM-related files if LLM is not enabled
if(NOT CVEDIX_WITH_LLM)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*mllm.*")
endif()

# (RKNN/TRT exclusions handled by default above for Core)

# Exclude GStreamer-related files if GStreamer is not enabled
if(NOT CVEDIX_WITH_GSTREAMER)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*rtsp.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*rtmp.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*image.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*file.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*screen.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*udp.*")
endif()

# Exclude FFmpeg-related files if FFmpeg is not enabled
if(NOT CVEDIX_WITH_FFMPEG)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*ffio.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*ff_.*")
    list(FILTER CORE_NODES EXCLUDE REGEX ".*cvedix_ff.*")
endif()

# Exclude license-related files if license is not enabled
if(NOT CVEDIX_WITH_LICENSE)
    list(FILTER CORE_UTILS EXCLUDE REGEX ".*license.*")
endif()

# Exclude ONNX Runtime-based nodes if ORT is not available
if(NOT CVEDIX_WITH_ORT)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*_ort_node.*")
endif()

# Exclude SeetaFace-based nodes if SeetaFace6 is not built
if(NOT CVEDIX_WITH_SEETAFACE)
    list(FILTER CORE_NODES EXCLUDE REGEX ".*face_recognizer_node.*")
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
add_library(cvedix_core SHARED ${CORE_NODES} ${CORE_UTILS} ${OBJECTS})
target_link_libraries(cvedix_core
    PUBLIC
        ${CVEDIX_DEPEND_LIBS}
        dl
    PRIVATE
        tinyexpr
)

# Alias
add_library(${PROJECT_NAME} ALIAS cvedix_core)

# --- PLUGINS ---

# RKNN Plugin
if(CVEDIX_WITH_RKNN)
    file(GLOB_RECURSE RKNN_PLUGIN_NODES "nodes/*rknn*.cpp")
    file(GLOB_RECURSE RKNN_PLUGIN_UTILS "utils/rknn/*.cpp" "utils/rga/*.cpp")
    
    # Only build if we have sources
    if(RKNN_PLUGIN_NODES OR RKNN_PLUGIN_UTILS)
        add_library(cvedix_backend_rknn MODULE ${RKNN_PLUGIN_NODES} ${RKNN_PLUGIN_UTILS})
        target_link_libraries(cvedix_backend_rknn cvedix_core ${CVEDIX_DEPEND_LIBS} rknnrt rga)
        target_compile_definitions(cvedix_backend_rknn PRIVATE CVEDIX_WITH_RKNN CVEDIX_WITH_RGA)
        
        # Output to plugins dir
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
        set_target_properties(cvedix_backend_rknn PROPERTIES 
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
            PREFIX "lib"
        )
        install(TARGETS cvedix_backend_rknn DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix/plugins)
    endif()
endif()


# Link licensecxx if license checking is enabled
if(CVEDIX_WITH_LICENSE)
    if(TARGET lcxx::lcxx)
        # Use PRIVATE to avoid export issues (licensecxx won't be exported)
        # This prevents CMake errors about absolute paths and missing nlohmann_json export
        # PRIVATE linking still provides include directories for compilation, just not for export
        target_link_libraries(cvedix_core PRIVATE lcxx::lcxx)
        message(STATUS "Linked cvedix_core with licensecxx (lcxx::lcxx)")
        
        # Link with identifiers for hardware binding support
        if(TARGET lcxx::identifiers)
            target_link_libraries(cvedix_core PRIVATE lcxx::identifiers)
            message(STATUS "Linked cvedix_core with licensecxx identifiers (hardware binding support)")
        endif()
    endif()
endif()

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
# Note: licensecxx is linked PRIVATE so it doesn't need to be exported
# This avoids issues with absolute paths in INTERFACE_INCLUDE_DIRECTORIES and nlohmann_json dependency
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
set(CVEDIX_CONFIG_DEFINES "")
if(CVEDIX_WITH_RKNN)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_RKNN)\n")
endif()
if(CVEDIX_WITH_RGA)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_RGA)\n")
endif()
if(CVEDIX_WITH_GSTREAMER)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_GSTREAMER)\n")
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
if(CVEDIX_WITH_KAFKA)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_KAFKA)\n")
endif()
if(CVEDIX_WITH_LLM)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_LLM)\n")
endif()
if(CVEDIX_WITH_FFMPEG)
    set(CVEDIX_CONFIG_DEFINES "${CVEDIX_CONFIG_DEFINES}add_definitions(-DCVEDIX_WITH_FFMPEG)\n")
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

# build for tools (license generator, etc.)
if(CVEDIX_WITH_LICENSE)
    add_subdirectory(tools)
endif()

# build for benchmarks
add_subdirectory(benchmarks)


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