
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was cvedix-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

# Find dependencies
find_dependency(OpenCV)

find_dependency(PkgConfig)

if(ON)
    find_dependency(OpenSSL)
endif()

find_dependency(Eigen3)


include("${CMAKE_CURRENT_LIST_DIR}/cvedix-targets.cmake")

# Export compile definitions that were used when building the SDK
# These need to be set in the consuming project to match the SDK build configuration
add_definitions(-DCVEDIX_WITH_LLM)


check_required_components(cvedix_instance_sdk)
