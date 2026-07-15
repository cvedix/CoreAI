# Install script for directory: /home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/llama.cpp/ggml/src/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml.so.0.15.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml.so.0.15.3"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml.so.0"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml.so.0.15.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHANGE
           FILE "${file}"
           OLD_RPATH "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin:"
           NEW_RPATH "")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-cpu.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-alloc.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-backend.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-blas.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-cann.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-cpp.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-cuda.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-opt.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-metal.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-rpc.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-virtgpu.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-sycl.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-vulkan.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-webgpu.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-zendnn.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/ggml-openvino.h"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party/llama.cpp/ggml/include/gguf.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml-base.so.0.15.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml-base.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml-base.so.0.15.3"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml-base.so.0"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml-base.so.0.15.3"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libggml-base.so.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin/libggml-base.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ggml" TYPE FILE FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/llama.cpp/ggml/ggml-config.cmake"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/llama.cpp/ggml/ggml-version.cmake"
    )
endif()

