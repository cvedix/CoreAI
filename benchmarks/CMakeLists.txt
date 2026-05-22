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

if(CVEDIX_WITH_FACE)
    # Face Recognizer Benchmark: detect → mask check → landmarks → feature → match
    # Standalone: links directly against SeetaFace6 + OpenCV (no cvedix_core dependency)
    add_executable(benchmark_face_recognizer_sample "benchmark_face_recognizer_sample.cpp")
    target_compile_definitions(benchmark_face_recognizer_sample PRIVATE CVEDIX_WITH_FACE)
    target_include_directories(benchmark_face_recognizer_sample PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/seetaface6/build/include
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_SOURCE_DIR}/utils/face
    )
    target_link_directories(benchmark_face_recognizer_sample PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/seetaface6/build/lib
    )
    target_link_libraries(benchmark_face_recognizer_sample
        ${OpenCV_LIBS}
        SeetaFaceDetector600
        SeetaFaceLandmarker600
        SeetaFaceRecognizer610
        SeetaMaskDetector200
        tennis
        SeetaAuthorize
    )

    install(TARGETS 
        benchmark_face_recognizer_sample
        DESTINATION bin
        OPTIONAL
    )
endif()

if(CVEDIX_WITH_LLM)
    add_executable(benchmark_llm "benchmark_llm.cpp")
    target_include_directories(benchmark_llm PRIVATE 
        ${CMAKE_SOURCE_DIR}/third_party/llm_engine/include
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_link_libraries(benchmark_llm PRIVATE llm_engine ${PROJECT_NAME})

    install(TARGETS 
        benchmark_llm
        DESTINATION bin
        OPTIONAL
    )
endif()

