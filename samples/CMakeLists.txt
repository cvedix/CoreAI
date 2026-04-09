# can ONLY be called by parent CMakeLists.txt, because no environment settings for this script.#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

message("start build for simple samples...")
# save all exe to 'build/bin'

# Default sample
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(1-1-1_sample "1-1-1_sample.cpp")
target_link_libraries(1-1-1_sample ${PROJECT_NAME})

add_executable(1-1-N_sample "1-1-N_sample.cpp")
target_link_libraries(1-1-N_sample ${PROJECT_NAME})

add_executable(1-N-N_sample "1-N-N_sample.cpp")
target_link_libraries(1-N-N_sample ${PROJECT_NAME})

add_executable(app_des_sample "app_des_sample.cpp")
target_link_libraries(app_des_sample ${PROJECT_NAME})

add_executable(app_src_des_sample "app_src_des_sample.cpp")
target_link_libraries(app_src_des_sample ${PROJECT_NAME})

add_executable(cvedix_logger_sample "cvedix_logger_sample.cpp")
target_link_libraries(cvedix_logger_sample ${PROJECT_NAME})

add_executable(cvedix_test "cvedix_test.cpp")
target_link_libraries(cvedix_test ${PROJECT_NAME})

add_executable(frame_fusion_sample "frame_fusion_sample.cpp")
target_link_libraries(frame_fusion_sample ${PROJECT_NAME})

add_executable(image_des_sample "image_des_sample.cpp")
target_link_libraries(image_des_sample ${PROJECT_NAME})

add_executable(image_src_sample "image_src_sample.cpp")
target_link_libraries(image_src_sample ${PROJECT_NAME})

add_executable(interaction_with_pipe_sample "interaction_with_pipe_sample.cpp")
target_link_libraries(interaction_with_pipe_sample ${PROJECT_NAME})

add_executable(message_broker_sample "message_broker_sample.cpp")
target_link_libraries(message_broker_sample ${PROJECT_NAME})

add_executable(N-1-N_sample "N-1-N_sample.cpp")
target_link_libraries(N-1-N_sample ${PROJECT_NAME})

add_executable(N-N_sample "N-N_sample.cpp")
target_link_libraries(N-N_sample ${PROJECT_NAME})

# add_executable(openpose_sample "openpose_sample.cpp")
# target_link_libraries(openpose_sample ${PROJECT_NAME})

add_executable(record_sample "record_sample.cpp")
target_link_libraries(record_sample ${PROJECT_NAME})

# RTSP samples
add_executable(rtsp_ba_crossline_sample "rtsp_ba_crossline_sample.cpp")
target_link_libraries(rtsp_ba_crossline_sample ${PROJECT_NAME})

add_executable(rtsp_src_sample "rtsp_src_sample.cpp")
target_link_libraries(rtsp_src_sample ${PROJECT_NAME})

add_executable(rtsp_des_sample "rtsp_des_sample.cpp")
target_link_libraries(rtsp_des_sample ${PROJECT_NAME})

add_executable(sse_broker_sample "sse_broker_sample.cpp")
target_link_libraries(sse_broker_sample ${PROJECT_NAME})

# add_executable(video_restoration_sample "video_restoration_sample.cpp")
# target_link_libraries(video_restoration_sample ${PROJECT_NAME})

add_executable(wrong_way_detection_sample "wrong_way_detection_sample.cpp")
target_link_libraries(wrong_way_detection_sample ${PROJECT_NAME})

# Install all basic samples
install(TARGETS 
        1-1-1_sample
        1-1-N_sample
        1-N-N_sample
        app_des_sample
        app_src_des_sample
        cvedix_logger_sample
        cvedix_test
        frame_fusion_sample
        image_des_sample
        image_src_sample
        interaction_with_pipe_sample
        message_broker_sample
        N-1-N_sample
        N-N_sample
        record_sample
        rtsp_ba_crossline_sample
        rtsp_src_sample
        rtsp_des_sample
        sse_broker_sample
        wrong_way_detection_sample
        DESTINATION bin
        OPTIONAL
)

# TensorRT samples (requires CVEDIX_WITH_TRT=ON)
if(CVEDIX_WITH_TRT)
    add_executable(ba_accident_detection_sample "ba_accident_detection_sample.cpp")
    target_link_libraries(ba_accident_detection_sample ${PROJECT_NAME})

    add_executable(ba_area_enter_exit_sample "ba_area_enter_exit_sample.cpp")
    target_link_libraries(ba_area_enter_exit_sample ${PROJECT_NAME})

    add_executable(ba_crossline_sample "ba_crossline_sample.cpp")
    target_link_libraries(ba_crossline_sample ${PROJECT_NAME})

    add_executable(ba_crowding_sample "ba_crowding_sample.cpp")
    target_link_libraries(ba_crowding_sample ${PROJECT_NAME})

    add_executable(ba_multiline_crossline_test "ba_multiline_crossline_test.cpp")
    target_link_libraries(ba_multiline_crossline_test ${PROJECT_NAME})

    add_executable(ba_multiple_crossline_counting_sample "ba_multiple_crossline_counting_sample.cpp")
    target_link_libraries(ba_multiple_crossline_counting_sample ${PROJECT_NAME})

    add_executable(ba_speed_estimation_sample "ba_speed_estimation_sample.cpp")
    target_link_libraries(ba_speed_estimation_sample ${PROJECT_NAME})

    add_executable(ba_traffic_analysis_sample "ba_traffic_analysis_sample.cpp")
    target_link_libraries(ba_traffic_analysis_sample ${PROJECT_NAME})



    add_executable(firesmoke_detect_sample "firesmoke_detect_sample.cpp")
    target_link_libraries(firesmoke_detect_sample ${PROJECT_NAME})

    add_executable(multi_detectors_sample "multi_detectors_sample.cpp")
    target_link_libraries(multi_detectors_sample ${PROJECT_NAME})

    add_executable(obstacle_detect_sample "obstacle_detect_sample.cpp")
    target_link_libraries(obstacle_detect_sample ${PROJECT_NAME})

    add_executable(skip_sample "skip_sample.cpp")
    target_link_libraries(skip_sample ${PROJECT_NAME})

    install(TARGETS 
        ba_accident_detection_sample
        ba_area_enter_exit_sample
        ba_crossline_sample
        ba_crowding_sample
        ba_multiline_crossline_test
        ba_multiple_crossline_counting_sample
        ba_speed_estimation_sample
        ba_traffic_analysis_sample
        firesmoke_detect_sample
        multi_detectors_sample
        obstacle_detect_sample
        skip_sample
        DESTINATION bin
        OPTIONAL
    )

endif()

if(CVEDIX_WITH_OPENVINO)
    add_executable(ba_movement_sample "ba_movement_sample.cpp")
    target_link_libraries(ba_movement_sample ${PROJECT_NAME})
        install(TARGETS 
        ba_movement_sample
        DESTINATION bin
        OPTIONAL
    )
endif()




# samples depend on Kafka
if(CVEDIX_WITH_KAFKA)
    add_executable(message_broker_kafka_sample "message_broker_kafka_sample.cpp")
    target_link_libraries(message_broker_kafka_sample ${PROJECT_NAME})
    
    install(TARGETS message_broker_kafka_sample
            DESTINATION bin
            OPTIONAL)
endif()

# samples depend on MQTT (mandatory)
add_executable(mqtt_json_receiver_sample "mqtt_json_receiver_sample.cpp")
target_include_directories(mqtt_json_receiver_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(mqtt_json_receiver_sample ${PROJECT_NAME})

# Link with mosquitto
if(MOSQUITTO_FOUND)
    target_link_libraries(mqtt_json_receiver_sample ${MOSQUITTO_LIBRARIES})
else()
    find_library(MOSQUITTO_LIB NAMES mosquitto)
    if(MOSQUITTO_LIB)
        target_link_libraries(mqtt_json_receiver_sample ${MOSQUITTO_LIB})
    endif()
endif()

install(TARGETS mqtt_json_receiver_sample
        DESTINATION bin
        OPTIONAL)

add_executable(ba_crossline_mqtt_sample "ba_crossline_mqtt_sample.cpp")
target_include_directories(ba_crossline_mqtt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(ba_crossline_mqtt_sample ${PROJECT_NAME})

# Link with mosquitto
if(MOSQUITTO_FOUND)
    target_link_libraries(ba_crossline_mqtt_sample ${MOSQUITTO_LIBRARIES})
else()
    find_library(MOSQUITTO_LIB NAMES mosquitto)
    if(MOSQUITTO_LIB)
        target_link_libraries(ba_crossline_mqtt_sample ${MOSQUITTO_LIB})
    endif()
endif()

install(TARGETS ba_crossline_mqtt_sample
        DESTINATION bin
        OPTIONAL)

# samples depend on LLM
if(CVEDIX_WITH_LLM)
    add_executable(mllm_analyse_sample "mllm_analyse_sample.cpp")
    target_link_libraries(mllm_analyse_sample ${PROJECT_NAME})    

    add_executable(mllm_analyse_sample_openai "mllm_analyse_sample_openai.cpp")
    target_link_libraries(mllm_analyse_sample_openai ${PROJECT_NAME})
    
    install(TARGETS 
        mllm_analyse_sample
        mllm_analyse_sample_openai
        DESTINATION bin
        OPTIONAL)
endif()

# samples depend on both TRT and LLM
# if(CVEDIX_WITH_TRT AND CVEDIX_WITH_LLM)
#     add_executable(speed_violation_mllm_analysis_sample "speed_violation_mllm_analysis_sample.cpp")
#     target_link_libraries(speed_violation_mllm_analysis_sample ${PROJECT_NAME})
#     
#     install(TARGETS speed_violation_mllm_analysis_sample
#             DESTINATION bin
#             OPTIONAL)
# endif()

# samples depend on FFmpeg (mandatory)
add_executable(ffmpeg_src_des_sample "ffmpeg_src_des_sample.cpp")
target_link_libraries(ffmpeg_src_des_sample ${PROJECT_NAME})

add_executable(ffmpeg_transcode_sample "ffmpeg_transcode_sample.cpp")
target_link_libraries(ffmpeg_transcode_sample ${PROJECT_NAME})

install(TARGETS 
    ffmpeg_src_des_sample
    ffmpeg_transcode_sample
    DESTINATION bin
    OPTIONAL)

# samples depend on RKNN
if(CVEDIX_WITH_RKNN)
    add_executable(rknn_yolov11_detector_sample "rknn_yolov11_detector_sample.cpp")
    target_link_libraries(rknn_yolov11_detector_sample ${PROJECT_NAME})

#     add_executable(rknn_face_detector_sample "rknn_face_detector_sample.cpp")
#     target_link_libraries(rknn_face_detector_sample ${PROJECT_NAME})

#     add_executable(rknn_yolov8_face_detection_simple_sample "rknn_yolov8_face_detection_simple_sample.cpp")
#     target_link_libraries(rknn_yolov8_face_detection_simple_sample ${PROJECT_NAME})

#     add_executable(rknn_yolov8_face_rtsp_sample "rknn_yolov8_face_rtsp_sample.cpp")
#     target_link_libraries(rknn_yolov8_face_rtsp_sample ${PROJECT_NAME})

#     add_executable(rknn_face_detection_sample "rknn_face_detection_sample.cpp")
#     target_link_libraries(rknn_face_detection_sample ${PROJECT_NAME})

#     add_executable(rknn_face_detection_file_sample "rknn_face_detection_file_sample.cpp")
#     target_link_libraries(rknn_face_detection_file_sample ${PROJECT_NAME})

#     add_executable(rknn_rtsp_tracking_sample "rknn_rtsp_tracking_sample.cpp")
#     target_link_libraries(rknn_rtsp_tracking_sample ${PROJECT_NAME})

#     add_executable(rknn_rtsp_tracking_mqtt_sample "rknn_rtsp_tracking_mqtt_sample.cpp")
#     target_include_directories(rknn_rtsp_tracking_mqtt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
#     target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${PROJECT_NAME})
    
#     # Link with mosquitto (MQTT is mandatory)
#     if(MOSQUITTO_FOUND)
#         target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${MOSQUITTO_LIBRARIES})
#     else()
#         find_library(MOSQUITTO_LIB NAMES mosquitto)
#         if(MOSQUITTO_LIB)
#             target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${MOSQUITTO_LIB})
#         endif()
#     endif()

#     # RKNN face tracking sample (MQTT is mandatory)
#     add_executable(rknn_face_tracking_sample "rknn_face_tracking_sample.cpp")
#     target_include_directories(rknn_face_tracking_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
#     target_link_libraries(rknn_face_tracking_sample ${PROJECT_NAME})
#
#     # Link with mosquitto for MQTT
#     if(MOSQUITTO_FOUND)
#         target_link_libraries(rknn_face_tracking_sample ${MOSQUITTO_LIBRARIES})
#     else()
#         find_library(MOSQUITTO_LIB NAMES mosquitto)
#         if(MOSQUITTO_LIB)
#             target_link_libraries(rknn_face_tracking_sample ${MOSQUITTO_LIB})
#         endif()
#     endif()
#
#     # Install RKNN face tracking sample
#     install(TARGETS rknn_face_tracking_sample DESTINATION bin)

#     # Install RKNN samples
#     install(TARGETS 
#         rknn_yolov11_detector_sample
#         rknn_face_detector_sample
#         rknn_yolov8_face_detection_simple_sample
#         rknn_yolov8_face_rtsp_sample
#         rknn_face_detection_sample
#         rknn_face_detection_file_sample
#         rknn_rtsp_tracking_sample
#         DESTINATION bin
#     )
    
#     # Install RKNN MQTT sample
#     install(TARGETS 
#         rknn_rtsp_tracking_mqtt_sample
#         DESTINATION bin
#         OPTIONAL
#     )
endif()

# samples depend on PaddlePaddle
if(CVEDIX_WITH_PADDLE)
    add_executable(app_src_sample "app_src_sample.cpp")
    target_link_libraries(app_src_sample ${PROJECT_NAME})
    
    add_executable(paddle_infer_sample "paddle_infer_sample.cpp")
    target_link_libraries(paddle_infer_sample ${PROJECT_NAME})
    
    install(TARGETS 
        app_src_sample
        paddle_infer_sample
        DESTINATION bin
        OPTIONAL)
endif()

# samples depend on CUDA(maybe other dependency required too)
if(CVEDIX_WITH_CUDA)
    # nvdec & nvenc plugins required from DeepStream
    add_executable(nv_hard_codec_sample "nv_hard_codec_sample.cpp")
    target_link_libraries(nv_hard_codec_sample ${PROJECT_NAME})
    
    install(TARGETS nv_hard_codec_sample
            DESTINATION bin
            OPTIONAL)
endif()

# samples depend on TensorRT
if(CVEDIX_WITH_TRT_FACE)
    # TensorRT YOLOv11 Plate Detector sample
    add_executable(yolov11_plate_detector_trt_sample "yolov11_plate_detector_trt_sample.cpp")
    target_include_directories(yolov11_plate_detector_trt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_plate_detector_trt_sample ${PROJECT_NAME})

    # YOLOv11 Plate Detection + ByteTrack Tracking sample with Analysis Board
    add_executable(yolov11_plate_bytetrack_sample "yolov11_plate_bytetrack_sample.cpp")
    target_include_directories(yolov11_plate_bytetrack_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_plate_bytetrack_sample ${PROJECT_NAME})
    
    # Install TensorRT samples (only working ones, vehicle/yolov8 samples disabled)
    install(TARGETS 
        # ba_jam_sample                                    # DISABLED - needs trt_vehicle
        # ba_stop_sample                                   # DISABLED - needs trt_vehicle
        # body_scan_and_plate_detect_sample                # DISABLED - needs trt_vehicle
        # dynamic_pipeline_sample                          # DISABLED - needs trt_vehicle
        # dynamic_pipeline_sample2                         # DISABLED - needs trt_vehicle
        # message_broker_sample2                           # DISABLED - needs trt_vehicle
        # plate_recognize_sample                           # DISABLED - needs trt_vehicle
        # src_des_sample                                   # DISABLED - needs trt_vehicle
        # trt_infer_sample                                 # DISABLED - needs trt_vehicle
        # vehicle_body_scan_sample                         # DISABLED - needs trt_vehicle
        # vehicle_cluster_based_on_classify_encoding_sample # DISABLED - needs trt_vehicle
        # vehicle_tracking_sample                          # DISABLED - needs trt_vehicle
        # 1-N-1_sample                                     # DISABLED - needs trt_vehicle
        # 1-N-1_sample2                                    # DISABLED - needs trt_vehicle
        # 1-N-1_sample3                                    # DISABLED - needs trt_vehicle
        # trt_yolov8_sample                                # DISABLED - needs trt_yolov8
        # trt_yolov8_sample2                               # DISABLED - needs trt_yolov8
        # rtmp_src_sample                                  # DISABLED - needs trt_vehicle
        # insightface_trt_sample
        # insightface_trt_simple_sample                    # DISABLED - file does not exist
        # insightface_register_recognize_face_trt_sample
        DESTINATION bin
        OPTIONAL
    )
endif()

if(CVEDIX_WITH_TRT AND CVEDIX_WITH_PADDLE)
    add_executable(plate_recognition_pipeline_sample "plate_recognition_pipeline_sample.cpp")
    target_include_directories(plate_recognition_pipeline_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(plate_recognition_pipeline_sample ${PROJECT_NAME})
    
    add_executable(plate_recognition_video_output_sample "plate_recognition_video_output_sample.cpp")
    target_include_directories(plate_recognition_video_output_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(plate_recognition_video_output_sample ${PROJECT_NAME})
    
    # Plate Detection + ByteTrack + PaddleOCR with Best Plate Selection
    add_executable(plate_bytetrack_ocr_sample "plate_bytetrack_ocr_sample.cpp")
    target_include_directories(plate_bytetrack_ocr_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(plate_bytetrack_ocr_sample ${PROJECT_NAME})
    
    install(TARGETS plate_recognition_pipeline_sample plate_recognition_video_output_sample plate_bytetrack_ocr_sample DESTINATION bin OPTIONAL)
endif()

if(CVEDIX_BUILD_COMPLEX_SAMPLES)
    message("start build for complex sampels...")
    # save exe to 'face_recognize'
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/face_recognize)
    add_executable(face_recognize_pipeline "face_recognize/face_recognize_pipeline.cpp")
    target_link_libraries(face_recognize_pipeline ${PROJECT_NAME})

    if(CVEDIX_WITH_TRT) # samples depend on TensorRT   
        # save exe to 'similiarity_search'
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/similiarity_search)
        add_executable(face_encoding_pipeline "similiarity_search/face_encoding_pipeline.cpp")
        target_link_libraries(face_encoding_pipeline ${PROJECT_NAME})
        add_executable(vehicle_encoding_pipeline "similiarity_search/vehicle_encoding_pipeline.cpp")
        target_link_libraries(vehicle_encoding_pipeline ${PROJECT_NAME})

        # save exe to 'vehicle_behaviour_analysis'
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vehicle_behaviour_analysis)
        add_executable(vehicle_ba_pipeline "vehicle_behaviour_analysis/vehicle_ba_pipeline.cpp")
        target_link_libraries(vehicle_ba_pipeline ${PROJECT_NAME})

        # save exe to 'vehicle_property_and_similiarity_search'
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vehicle_property_and_similiarity_search)
        add_executable(vehicle_encoding_classify_pipeline "vehicle_property_and_similiarity_search/vehicle_encoding_classify_pipeline.cpp")
        target_link_libraries(vehicle_encoding_classify_pipeline ${PROJECT_NAME})

        # save exe to 'lpr_camera'
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lpr_camera)
        add_executable(plate_recognize_pipeline "lpr_camera/plate_recognize_pipeline.cpp")
        target_link_libraries(plate_recognize_pipeline ${PROJECT_NAME})
    endif()

    if(CVEDIX_WITH_PADDLE)
        # save exe to 'math_review'
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/math_review)
        add_executable(math_expression_check_pipeline "math_review/math_expression_check_pipeline.cpp")
        target_link_libraries(math_expression_check_pipeline ${PROJECT_NAME}) 
    endif()
    
endif()