cmake_minimum_required(VERSION 3.10)
project(ov_yolov26 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# OpenVINO required
find_package(OpenVINO REQUIRED)

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include backend interface
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../nodes/infers/backend)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build library
add_library(${PROJECT_NAME} SHARED "ov_yolov26_detector.cpp" "ov_yolov26_backend.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE openvino::runtime ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
)
