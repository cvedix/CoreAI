cmake_minimum_required(VERSION 3.10)
project(cvedix_yolo_ort_detector VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include backend interface
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../nodes/infers/backend)

# ONNX Runtime
set(ONNXRUNTIME_DIR ${CMAKE_SOURCE_DIR}/third_party/onnxruntime)
if(EXISTS ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)
    include_directories(${ONNXRUNTIME_DIR}/include)
    link_directories(${ONNXRUNTIME_DIR}/lib)
endif()

# Build library
add_library(${PROJECT_NAME} SHARED
    "cvedix_yolo_ort_detector.cpp"
    "cvedix_yolo_ort_detector_backend.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix)