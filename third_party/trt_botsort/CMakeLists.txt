cmake_minimum_required(VERSION 3.10)
project(trt_botsort VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# CUDA required
find_package(CUDA REQUIRED)
include_directories(${CUDA_INCLUDE_DIRS})
link_directories(${CUDA_TOOLKIT_ROOT_DIR}/lib64)

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Eigen required
set(EIGEN_VERSION 3.3)
find_package(Eigen3 ${EIGEN_VERSION} QUIET)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/TRT_InferenceEngine)

# Glob source files
file(GLOB_RECURSE SOURCE_FILES 
    "*.cpp"
    "TRT_InferenceEngine/*.cpp"
)

# Build library
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

# Link libraries
# Note: assuming libnvinfer.so is in standard system path or LD_LIBRARY_PATH
target_link_libraries(${PROJECT_NAME} Eigen3::Eigen nvinfer nvonnxparser cudart ${OpenCV_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME} EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
