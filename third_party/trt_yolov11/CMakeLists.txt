cmake_minimum_required(VERSION 3.10)
project(trt_yolov11 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# CUDA required
find_package(CUDA REQUIRED)
include_directories(${CUDA_INCLUDE_DIRS})

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build library
add_library(${PROJECT_NAME} SHARED "trt_yolov11_detector.cpp")

# Link libraries
# Note: assuming libnvinfer.so is in standard system path or LD_LIBRARY_PATH
target_link_libraries(${PROJECT_NAME} nvinfer cudart ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME} EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
