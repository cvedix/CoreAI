# CVEDIX C API — stable C ABI for language bindings (Java/JNA, C#/P-Invoke, C)
# Can ONLY be called by parent CMakeLists.txt.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

add_library(cvedix_capi SHARED src/cvedix_c_api.cpp)

target_include_directories(cvedix_capi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cvedix/capi>
)

target_link_libraries(cvedix_capi PRIVATE cvedix_core)

# Hide C++ symbols — only the extern "C" surface is exported.
set_target_properties(cvedix_capi PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS cvedix_capi EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cvedix
)
install(FILES include/cvedix_c_api.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cvedix/capi
)
