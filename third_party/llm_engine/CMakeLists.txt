#[[
    LLM Engine — C++ wrapper around llama.cpp for CVEDIX SDK
    
    This library provides a clean, pipeline-friendly API for local LLM inference
    using GGUF models with CUDA acceleration.
    
    Requirements:
    - CUDA toolkit
    - llama.cpp (built as part of parent CMake)
    
    Usage:
        target_link_libraries(your_target PRIVATE llm_engine)
]]

cmake_minimum_required(VERSION 3.20)
project(llm_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Source files
set(LLM_SOURCES
    src/llm_engine.cpp
    src/llm_sampler.cpp
    src/llm_config.cpp
    src/llm_c_api.cpp
)

add_library(llm_engine STATIC ${LLM_SOURCES})

target_include_directories(llm_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against llama.cpp targets (built by parent CMake)
# llama.cpp exposes 'llama' and 'ggml' targets
target_link_libraries(llm_engine PRIVATE
    llama
    ggml
)

# Propagate CUDA include directories privately
target_include_directories(llm_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

# Position-independent code for shared library embedding
set_target_properties(llm_engine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# HTTP Server
add_executable(llm_server src/llm_server.cpp)
target_include_directories(llm_server PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/vendor
)
target_link_libraries(llm_server PRIVATE llm_engine cpp-httplib)

# C API Unit Test
add_executable(llm_c_api_test tests/test_llm_c_api.c)
target_link_libraries(llm_c_api_test PRIVATE llm_engine)
# Make sure C compiler uses C++ standard libraries for the wrapper
set_target_properties(llm_c_api_test PROPERTIES LINKER_LANGUAGE CXX)
