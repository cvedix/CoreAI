cmake_minimum_required(VERSION 3.10)
project(onnx_yolov11 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# OpenCV required (for cv::dnn)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include backend interface
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../nodes/infers/backend)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build library (CPU-based, no CUDA required)
add_library(${PROJECT_NAME} SHARED "onnx_yolov11_detector.cpp" "onnx_yolov11_backend.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
