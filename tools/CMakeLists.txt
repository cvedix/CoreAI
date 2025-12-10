# Tools CMakeLists.txt
# Build license generator tool

if(CVEDIX_WITH_LICENSE)
    # License generator executable
    add_executable(license_generator license_generator.cpp)
    
    # Link with licensecxx and identifiers
    if(TARGET lcxx::lcxx)
        target_link_libraries(license_generator PRIVATE lcxx::lcxx lcxx::identifiers)
        message(STATUS "License generator tool will be built")
    else()
        message(WARNING "lcxx::lcxx target not found - license generator will not be built")
    endif()
    
    # Hardware ID tool executable
    add_executable(get_hardware_id get_hardware_id.cpp)
    
    # Link with identifiers
    if(TARGET lcxx::identifiers)
        target_link_libraries(get_hardware_id PRIVATE lcxx::identifiers)
        message(STATUS "Hardware ID tool will be built")
    else()
        message(WARNING "lcxx::identifiers target not found - hardware ID tool will not be built")
    endif()
    
    # Install tools
    install(TARGETS license_generator get_hardware_id
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

