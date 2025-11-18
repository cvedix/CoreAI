#[[
    prepare in advance：
    1. OpenCV >= 4.6
    2. Gstreamer >= 1.14.5
    3. CUDA, TensorRT, PADDLE and others for optional
]]

cmake_minimum_required(VERSION 3.10)
project(video_pipe VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -w -fdiagnostics-color=always -pthread")
# save all libs(including third_party's) to 'libs'
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)

# optional for build, modify values when configure the project using 'cmake -DCVEDIX_WITH_CUDA=OFF ..'
option(CVEDIX_WITH_CUDA "prepared CUDA or not?" OFF)
option(CVEDIX_WITH_TRT "prepared TensorRT or not?" OFF)
option(CVEDIX_WITH_PADDLE "prepared PaddlePaddle or not?" OFF)
option(CVEDIX_WITH_KAFKA "prepared Kafka or not?" OFF)
option(CVEDIX_WITH_LLM "prepared LLM or not?" OFF)
option(CVEDIX_WITH_FFMPEG "prepared FFMPEG or not?" OFF)
option(CVEDIX_WITH_RKNN "prepared RKNN (Rockchip NPU) or not?" OFF)
option(CVEDIX_WITH_RGA "prepared RGA (Rockchip Graphics Accelerator) or not?" OFF)
option(CVEDIX_BUILD_COMPLEX_SAMPLES "build complex samples or not? (maybe source code not provided)" OFF)

# OpenCV required
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
include_directories(${OpenCV_INCLUDE_DIRS})

# Gstreamer required
include(FindPkgConfig)
pkg_check_modules(GST   REQUIRED gstreamer-1.0)
pkg_check_modules(GSTAPP REQUIRED gstreamer-app-1.0)
# RTSP server is optional - only needed for RTSP nodes
pkg_check_modules(GST_RTSP gstreamer-rtsp-server-1.0)
if(GST_RTSP_FOUND)
    message(STATUS "GStreamer RTSP server found - RTSP nodes will be available")
    add_definitions(-DCVEDIX_WITH_RTSP_SERVER)
else()
    message(WARNING "GStreamer RTSP server not found - RTSP nodes will be disabled")
    message(WARNING "  Install with: sudo apt-get install libgstrtspserver-1.0-dev gstreamer1.0-rtsp")
    set(GST_RTSP_LIBRARIES "")
endif()
message(STATUS "GStreamer library status:")
message(STATUS "    version: ${GST_VERSION}")
message(STATUS "    libraries: ${GST_LIBRARIES} ${GSTAPP_LIBRARIES} ${GST_RTSP_LIBRARIES}")
message(STATUS "    include path: ${GST_INCLUDE_DIRS}")
include_directories(${GST_INCLUDE_DIRS})
set (GST_DEPEND_LIBS ${GST_LIBRARIES} ${GSTAPP_LIBRARIES})
if(GST_RTSP_FOUND)
    list(APPEND GST_DEPEND_LIBS ${GST_RTSP_LIBRARIES})
endif()

# collect dependent libs for videopipe
list(APPEND CVEDIX_DEPEND_LIBS ${OpenCV_LIBS} ${GST_DEPEND_LIBS} stdc++fs)

# optional for CUDA
if(CVEDIX_WITH_CUDA) # CUDA enabled
    add_definitions(-DCVEDIX_WITH_CUDA)
endif()

# optional for TensorRT
if(CVEDIX_WITH_TRT)  # TensorRT enabled
    add_definitions(-DCVEDIX_WITH_TRT)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project
    set(CVEDIX_TRT_LIB_PATH "")
    set(CVEDIX_TRT_INC_PATH "")
    # trt_vehicle
    message("-------------start build trt_vehicle--------------")
    add_subdirectory(third_party/trt_vehicle)
    list(APPEND CVEDIX_DEPEND_LIBS trt_vehicle)
    link_directories(${CVEDIX_TRT_LIB_PATH})
    message(STATUS "TensorRT library status:")
    message(STATUS "    include path: ${CVEDIX_TRT_INC_PATH}")
    message(STATUS "    library path: ${CVEDIX_TRT_LIB_PATH}")
    message("--------------end build trt_vehicle---------------")
    # trt_yolov8    
    message("-------------start build trt_yolov8--------------")
    add_subdirectory(third_party/trt_yolov8)
    list(APPEND CVEDIX_DEPEND_LIBS trt_yolov8)
    message("--------------end build trt_yolov8---------------")
endif()

# optional for PaddlePaddle
if(CVEDIX_WITH_PADDLE)  # PaddlePaddle enabled
    add_definitions(-DCVEDIX_WITH_PADDLE)
    set(CVEDIX_BUILD_FROM ON)  # set flag for sub project
    set(CVEDIX_PADDLE_LIB_PATH "")
    set(CVEDIX_PADDLE_INC_PATH "")
    message("-------------start build paddle_ocr--------------")
    add_subdirectory(third_party/paddle_ocr) # build paddle_ocr
    list(APPEND CVEDIX_DEPEND_LIBS paddle_ocr)
    link_directories(${CVEDIX_PADDLE_LIB_PATH})
    message(STATUS "PaddlePaddle library status:")
    message(STATUS "    include path: ${CVEDIX_PADDLE_INC_PATH}")
    message(STATUS "    library path: ${CVEDIX_PADDLE_LIB_PATH}")
    message("--------------end build paddle_ocr---------------")
endif()

# optional for Kafka
if(CVEDIX_WITH_KAFKA)
    add_definitions(-DCVEDIX_WITH_KAFKA)
    list(APPEND CVEDIX_DEPEND_LIBS rdkafka++)
endif()

# optional for LLM
if(CVEDIX_WITH_LLM)
    find_package(OpenSSL REQUIRED)
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

# optional for RKNN (Rockchip NPU)
if(CVEDIX_WITH_RKNN)
    add_definitions(-DCVEDIX_WITH_RKNN)
    # Find librknnrt.so - typically installed in /usr/local/lib or /usr/lib
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
    message(STATUS "RKNN support: enabled")
endif()

# optional for RGA (Rockchip Graphics Accelerator)
if(CVEDIX_WITH_RGA)
    add_definitions(-DCVEDIX_WITH_RGA)
    # Find librga.so
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
    message(STATUS "RGA support: enabled")
endif()

# tinyexpr
message("-------------start build tinyexpr--------------")
add_subdirectory(third_party/tinyexpr)
list(APPEND CVEDIX_DEPEND_LIBS tinyexpr)
message("--------------end build tinyexpr---------------")

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

# collect source files for videopipe
file(GLOB_RECURSE NODES "nodes/*.cpp")
file(GLOB_RECURSE OBJECTS "objects/*.cpp")
file(GLOB_RECURSE UTILS "utils/*.cpp")
#...#
list(APPEND CVEDIX_CPPS_SOURCES ${NODES} ${OBJECTS} ${UTILS})

# build for videopipe
add_library(${PROJECT_NAME} SHARED ${CVEDIX_CPPS_SOURCES})
target_link_libraries(${PROJECT_NAME} ${CVEDIX_DEPEND_LIBS})

# build for samples
add_subdirectory(samples)