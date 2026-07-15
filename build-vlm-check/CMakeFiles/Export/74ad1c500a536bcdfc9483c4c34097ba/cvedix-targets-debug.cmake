#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "cvedix::tinyexpr" for configuration "Debug"
set_property(TARGET cvedix::tinyexpr APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cvedix::tinyexpr PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cvedix/libtinyexpr.so"
  IMPORTED_SONAME_DEBUG "libtinyexpr.so"
  )

list(APPEND _cmake_import_check_targets cvedix::tinyexpr )
list(APPEND _cmake_import_check_files_for_cvedix::tinyexpr "${_IMPORT_PREFIX}/lib/cvedix/libtinyexpr.so" )

# Import target "cvedix::cvedix_core" for configuration "Debug"
set_property(TARGET cvedix::cvedix_core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cvedix::cvedix_core PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "cvedix::tinyexpr"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_core.so"
  IMPORTED_SONAME_DEBUG "libcvedix_core.so"
  )

list(APPEND _cmake_import_check_targets cvedix::cvedix_core )
list(APPEND _cmake_import_check_files_for_cvedix::cvedix_core "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_core.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
