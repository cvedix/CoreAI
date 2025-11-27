cmake_minimum_required(VERSION 3.10)
project(opencv_zoo_face_detection_yunet)

set(OPENCV_VERSION "4.10.0")
set(OPENCV_INSTALLATION_PATH "" CACHE PATH "Where to look for OpenCV installation")

# Find OpenCV
find_package(OpenCV ${OPENCV_VERSION} REQUIRED HINTS ${OPENCV_INSTALLATION_PATH})

# OpenCV demo (original)
add_executable(demo demo.cpp)
target_link_libraries(demo ${OpenCV_LIBS})

# RKNN YuNet demo
if(CVEDIX_WITH_RKNN)
    # Find RKNN library - try multiple paths
    find_library(RKNN_LIB 
        NAMES rknnrt
        PATHS ${CMAKE_SOURCE_DIR}/third_party/rknn/lib 
              ${CMAKE_SOURCE_DIR}/third_party/rknn/libs
              /usr/local/lib 
              /usr/lib 
              /opt/rockchip/lib
        NO_DEFAULT_PATH
    )
    
    # If not found in specific paths, try standard paths
    if(NOT RKNN_LIB)
        find_library(RKNN_LIB NAMES rknnrt)
    endif()
    
    # Find RKNN headers
    find_path(RKNN_INCLUDE_DIR
        NAMES rknn_api.h
        PATHS ${CMAKE_SOURCE_DIR}/third_party/rknn/include 
              /usr/local/include 
              /usr/include 
              /opt/rockchip/include
        NO_DEFAULT_PATH
    )
    
    # If not found in specific paths, try standard paths
    if(NOT RKNN_INCLUDE_DIR)
        find_path(RKNN_INCLUDE_DIR NAMES rknn_api.h)
    endif()
    
    if(RKNN_LIB AND RKNN_INCLUDE_DIR)
        add_executable(rknn_yunet_demo rknn_yunet_demo.cpp)
        
        target_include_directories(rknn_yunet_demo PRIVATE
            ${RKNN_INCLUDE_DIR}
            ${OpenCV_INCLUDE_DIRS}
        )
        
        target_link_libraries(rknn_yunet_demo
            ${RKNN_LIB}
            ${OpenCV_LIBS}
        )
        
        message(STATUS "RKNN YuNet demo will be built")
        message(STATUS "  RKNN library: ${RKNN_LIB}")
        message(STATUS "  RKNN include: ${RKNN_INCLUDE_DIR}")
    else()
        if(NOT RKNN_LIB)
            message(WARNING "RKNN library (librknnrt.so) not found, skipping rknn_yunet_demo")
            message(WARNING "  Please install rknn-toolkit2 or set RKNN library path")
        endif()
        if(NOT RKNN_INCLUDE_DIR)
            message(WARNING "RKNN headers (rknn_api.h) not found, skipping rknn_yunet_demo")
            message(WARNING "  Please install rknn-toolkit2 or set RKNN include path")
        endif()
    endif()
endif()