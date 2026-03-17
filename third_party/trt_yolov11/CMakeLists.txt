cmake_minimum_required(VERSION 3.10)
project(trt_yolov11 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# CUDA required
find_package(CUDA REQUIRED)
include_directories(${CUDA_INCLUDE_DIRS})
link_directories(${CUDA_TOOLKIT_ROOT_DIR}/lib64)

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include backend interface
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../nodes/infers/backend)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build library 
add_library(${PROJECT_NAME} SHARED "trt_yolov11_detector.cpp" "trt_yolov11_backend.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE nvinfer cudart ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)