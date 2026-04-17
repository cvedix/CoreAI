cmake_minimum_required(VERSION 3.10)
project(rknn_yolov26 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -w -fdiagnostics-color=always -pthread")

# RKNN required
find_path(RKNN_INCLUDE_DIR rknn_api.h 
    PATHS 
        ${CMAKE_SOURCE_DIR}/../rknn/include
        /usr/include 
        /usr/local/include
    ENV RKNN_INCLUDE_DIR)
find_library(RKNN_LIBRARY rknnrt 
    PATHS 
        ${CMAKE_SOURCE_DIR}/../rknn/libs
        /usr/lib 
        /usr/local/lib
    ENV RKNN_LIB_DIR)

if(NOT RKNN_INCLUDE_DIR OR NOT RKNN_LIBRARY)
    message(WARNING "RKNN not found, skipping rknn_yolov26 plugin")
    return()
endif()

include_directories(${RKNN_INCLUDE_DIR})

# OpenCV required
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include backend interface
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../nodes/infers/backend)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Check for RGA support (optional hardware acceleration for Rockchip)
# CVEDIX_WITH_RGA may be passed from parent CMakeLists
if(CVEDIX_WITH_RGA)
    find_path(RGA_INCLUDE_DIR rga/im2d.hpp 
        PATHS 
            ${CMAKE_SOURCE_DIR}/../librga/include
            /usr/include 
            /usr/local/include
        ENV RGA_INCLUDE_DIR)
    find_library(RGA_LIBRARY rga 
        PATHS 
            ${CMAKE_SOURCE_DIR}/../librga/libs
            /usr/lib 
            /usr/local/lib
        ENV RGA_LIB_DIR)

    if(RGA_INCLUDE_DIR AND RGA_LIBRARY)
        message(STATUS "RGA found, enabling RGA acceleration for RKNN")
        add_definitions(-DCVEDIX_WITH_RGA)
        include_directories(${RGA_INCLUDE_DIR})
        set(OPTIONAL_LIBS ${RGA_LIBRARY})
    else()
        message(STATUS "RGA not found, using CPU preprocessing for RKNN")
        set(OPTIONAL_LIBS "")
    endif()
else()
    message(STATUS "RGA support disabled (use -DCVEDIX_WITH_RGA=ON to enable)")
    set(OPTIONAL_LIBS "")
endif()

# Build library 
add_library(${PROJECT_NAME} SHARED 
    "rknn_yolov26.cpp" 
    "rknn_yolov26_backend.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${RKNN_LIBRARY} 
    ${OpenCV_LIBS}
    ${OPTIONAL_LIBS})

# Install rules
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
)
