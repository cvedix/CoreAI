cmake_minimum_required(VERSION 3.10)
project(asio VERSION 1.0)


add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(asio
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

