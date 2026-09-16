# can ONLY be called by parent CMakeLists.txt, because no environment settings for this script.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

message("start build for tests...")

find_package(Threads REQUIRED)

# Tests exercise the framework layer only (cvedix_node, the queues, attach_to),
# so they need no models, no video files and no GPU — they run anywhere the
# library builds, which is what makes them usable as a CI gate.
set(CVEDIX_TEST_TARGETS
    test_cvedix_node
    test_pipeline_smoke
)

foreach(test_target IN LISTS CVEDIX_TEST_TARGETS)
    add_executable(${test_target} "${test_target}.cpp")
    target_link_libraries(${test_target} PRIVATE ${PROJECT_NAME} Threads::Threads)
    # for the local "test_assert.h" / "test_nodes.h" helpers
    target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )

    add_test(NAME ${test_target} COMMAND ${test_target})
    # Threading tests poll with timeouts; a hung node must fail the run rather
    # than block CI forever.
    set_tests_properties(${test_target} PROPERTIES TIMEOUT 300)
endforeach()

# Tests are a development aid, not part of the shipped package, so they are
# deliberately NOT added to install(TARGETS ...).
