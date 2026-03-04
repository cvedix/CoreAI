# can ONLY be called by parent CMakeLists.txt, because no environment settings for this script.#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

message(STATUS "Building benchmarks...")
# save all exe to 'build/bin'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CVEDIX_WITH_TRT)
    # Pipeline Benchmark: det → track → ba_crossline (with video output)
    add_executable(benchmark_pipeline_sample "benchmark_pipeline_sample.cpp")
    target_include_directories(benchmark_pipeline_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(benchmark_pipeline_sample ${PROJECT_NAME})

    # People Analytics Benchmark: counting + crowd + loitering + wrong-way
    add_executable(benchmark_people_analytics_sample "benchmark_people_analytics_sample.cpp")
    target_include_directories(benchmark_people_analytics_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(benchmark_people_analytics_sample ${PROJECT_NAME})

    install(TARGETS 
        benchmark_pipeline_sample
        benchmark_people_analytics_sample
        DESTINATION bin
        OPTIONAL
    )
endif()
