# Install script for directory: /home/cvedix/workspace/rapidmedia/3rdpart/CoreAI

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

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix" TYPE SHARED_LIBRARY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/libs/libtinyexpr.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libtinyexpr.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix" TYPE SHARED_LIBRARY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/libs/libcvedix_core.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so"
         OLD_RPATH "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/libs:/usr/local/lib:/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/bin:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/libcvedix_core.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cvedix" TYPE DIRECTORY FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/nodes"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/objects"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/utils"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/excepts"
    FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cvedix" TYPE DIRECTORY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/third_party" FILES_MATCHING REGEX "/[^/]*\\.h$" REGEX "/[^/]*\\.hpp$" REGEX "/[^/]*\\.ipp$" REGEX "/\\.git$" EXCLUDE REGEX "/[^/]*\\.md$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cvedix" TYPE FILE FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/cvedix_version.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake/cvedix-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake/cvedix-targets.cmake"
         "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/CMakeFiles/Export/74ad1c500a536bcdfc9483c4c34097ba/cvedix-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake/cvedix-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake/cvedix-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake" TYPE FILE FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/CMakeFiles/Export/74ad1c500a536bcdfc9483c4c34097ba/cvedix-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake" TYPE FILE FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/CMakeFiles/Export/74ad1c500a536bcdfc9483c4c34097ba/cvedix-targets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix/cmake" TYPE FILE FILES
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/cvedix-config.cmake"
    "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/cvedix-config-version.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cvedix/pkgconfig" TYPE FILE FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/cvedix.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE DIRECTORY FILES "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/cvedix_data" FILES_MATCHING REGEX "/[^/]*$" REGEX "/\\.git$" EXCLUDE)
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/onnx_yolov11/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/onnx_yolov26/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/llama.cpp/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/llm_engine/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/tinyexpr/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/third_party/asio/cmake_install.cmake")
  include("/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/samples/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/cvedix/workspace/rapidmedia/3rdpart/CoreAI/build-vlm-check/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
