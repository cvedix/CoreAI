# can ONLY be called by parent CMakeLists.txt, because no environment settings for this script.#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

message("start build for simple samples...")
if(DEFINED MEDIATOOLKIT_LIB AND MEDIATOOLKIT_LIB)
    link_libraries(${MEDIATOOLKIT_LIB})
    set(MEDIATOOLKIT_LINK_LIB ${MEDIATOOLKIT_LIB})
else()
    link_libraries(MediaToolKit)
    set(MEDIATOOLKIT_LINK_LIB MediaToolKit)
endif()
# save all exe to 'build/bin'

# Default sample
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(ppe_video_sample "ppe_video_sample.cpp")
target_link_libraries(ppe_video_sample ${PROJECT_NAME})
add_dependencies(ppe_video_sample onnx_yolov11)
if(CVEDIX_WITH_TRT)
    add_dependencies(ppe_video_sample trt_yolov11)
endif()

add_executable(1-1-1_sample "1-1-1_sample.cpp")
target_link_libraries(1-1-1_sample ${PROJECT_NAME} ${MEDIATOOLKIT_LINK_LIB})

add_executable(1-1-N_sample "1-1-N_sample.cpp")
target_link_libraries(1-1-N_sample ${PROJECT_NAME})

add_executable(1-N-N_sample "1-N-N_sample.cpp")
target_link_libraries(1-N-N_sample ${PROJECT_NAME})

add_executable(1-N-1-N_sample "1-N-1-N_sample.cpp")
target_link_libraries(1-N-1-N_sample ${PROJECT_NAME})





add_executable(N-1-N_sample "N-1-N_sample.cpp")
target_link_libraries(N-1-N_sample ${PROJECT_NAME})

add_executable(N-N_sample "N-N_sample.cpp")
target_link_libraries(N-N_sample ${PROJECT_NAME})

# add_executable(openpose_sample "openpose_sample.cpp")
# target_link_libraries(openpose_sample ${PROJECT_NAME})



# webhook_broker_sample - needs API update
# add_executable(webhook_broker_sample "webhook_broker_sample.cpp")
# target_link_libraries(webhook_broker_sample ${PROJECT_NAME})

# add_executable(video_restoration_sample "video_restoration_sample.cpp")
# target_link_libraries(video_restoration_sample ${PROJECT_NAME})

# Install all basic samples
install(TARGETS 
        ppe_video_sample
        1-1-1_sample
        1-1-N_sample
        1-N-N_sample
        1-N-1-N_sample
        N-1-N_sample
        N-N_sample
        DESTINATION bin
        OPTIONAL
)

# TensorRT samples (requires CVEDIX_WITH_TRT=ON)
if(CVEDIX_WITH_TRT)
endif()

if(CVEDIX_WITH_OPENVINO)
endif()

# SeetaFace6 face analysis sample
if(CVEDIX_WITH_FACE)
endif()

# Milvus large-scale face search sample (requires SeetaFace6 + Milvus)
if(CVEDIX_WITH_FACE AND CVEDIX_WITH_MILVUS)
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

# samples depend on LLM
# if(CVEDIX_WITH_LLM)
#     add_executable(mllm_analyse_sample "mllm_analyse_sample.cpp")
#     target_link_libraries(mllm_analyse_sample ${PROJECT_NAME})    
# 
#     add_executable(mllm_analyse_sample_openai "mllm_analyse_sample_openai.cpp")
#     target_link_libraries(mllm_analyse_sample_openai ${PROJECT_NAME})
#     
#     install(TARGETS 
#         mllm_analyse_sample
#         mllm_analyse_sample_openai
#         DESTINATION bin
#         OPTIONAL)
# endif()

# samples depend on both TRT and LLM
# if(CVEDIX_WITH_TRT AND CVEDIX_WITH_LLM)
#     add_executable(speed_violation_mllm_analysis_sample "speed_violation_mllm_analysis_sample.cpp")
#     target_link_libraries(speed_violation_mllm_analysis_sample ${PROJECT_NAME})
#     
#     install(TARGETS speed_violation_mllm_analysis_sample
#             DESTINATION bin
#             OPTIONAL)
# endif()

# samples depend on LLM (local LLM inference via llama.cpp)
if(CVEDIX_WITH_LLM)
    add_executable(vlm_object_feature_sample "vlm_object_feature_sample.cpp")
    target_link_libraries(vlm_object_feature_sample ${PROJECT_NAME})

    add_executable(rapidmedia_vlm_feature_sample "rapidmedia_vlm_feature_sample.cpp")
    target_link_libraries(rapidmedia_vlm_feature_sample ${PROJECT_NAME})

    add_executable(event_snapshot_vlm_enrichment_sample "event_snapshot_vlm_enrichment_sample.cpp")
    target_link_libraries(event_snapshot_vlm_enrichment_sample ${PROJECT_NAME})

    install(TARGETS
            vlm_object_feature_sample
            rapidmedia_vlm_feature_sample
            event_snapshot_vlm_enrichment_sample
            DESTINATION bin
            OPTIONAL)
endif()

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
    set(PPOCR_NODE_HEADER "${PROJECT_SOURCE_DIR}/nodes/infers/cvedix_ppocr_text_detector_node.h")
    if(EXISTS ${PPOCR_NODE_HEADER})
        add_executable(paddle_infer_sample "paddle_infer_sample.cpp")
        target_link_libraries(paddle_infer_sample ${PROJECT_NAME})

        install(TARGETS 
            paddle_infer_sample
            DESTINATION bin
            OPTIONAL)
    endif()
endif()

# samples depend on CUDA(maybe other dependency required too)
if(CVEDIX_WITH_CUDA)
    # nvdec & nvenc plugins required from DeepStream
    # add_executable(nv_hard_codec_sample "nv_hard_codec_sample.cpp")
    # target_link_libraries(nv_hard_codec_sample ${PROJECT_NAME})
    
    # install(TARGETS nv_hard_codec_sample
    #         DESTINATION bin
    #         OPTIONAL)
endif()

# samples depend on TensorRT
if(CVEDIX_WITH_TRT_FACE)
    # TensorRT YOLOv11 Plate Detector sample
    add_executable(yolov11_plate_detector_trt_sample "yolov11_plate_detector_trt_sample.cpp")
    target_include_directories(yolov11_plate_detector_trt_sample PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_plate_detector_trt_sample ${PROJECT_NAME})

    # YOLOv11 Plate Detection + ByteTrack Tracking sample with Analysis Board
    add_executable(yolov11_plate_bytetrack_sample "yolov11_plate_bytetrack_sample.cpp")
    target_include_directories(yolov11_plate_bytetrack_sample PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
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
    set(TRT_YOLOV11_DET_HEADER "${PROJECT_SOURCE_DIR}/nodes/infers/cvedix_trt_yolov11_det_node.h")
    set(PLATE_PPOCR_HEADER "${PROJECT_SOURCE_DIR}/nodes/infers/cvedix_plate_recogniton_ppocr3.h")

    if(EXISTS ${TRT_YOLOV11_DET_HEADER} AND EXISTS ${PLATE_PPOCR_HEADER})
        add_executable(plate_recognition_pipeline_sample "plate_recognition_pipeline_sample.cpp")
        target_include_directories(plate_recognition_pipeline_sample PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
        target_link_libraries(plate_recognition_pipeline_sample ${PROJECT_NAME})
        
        add_executable(plate_recognition_video_output_sample "plate_recognition_video_output_sample.cpp")
        target_include_directories(plate_recognition_video_output_sample PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
        target_link_libraries(plate_recognition_video_output_sample ${PROJECT_NAME})

        install(TARGETS plate_recognition_pipeline_sample plate_recognition_video_output_sample DESTINATION bin OPTIONAL)
    else()
        message(STATUS "Plate recognition TRT samples disabled - headers not found")
    endif()

    if(EXISTS ${PLATE_PPOCR_HEADER})
        # Plate Detection + ByteTrack + PaddleOCR with Best Plate Selection
        add_executable(plate_bytetrack_ocr_sample "plate_bytetrack_ocr_sample.cpp")
        target_include_directories(plate_bytetrack_ocr_sample PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
        target_link_libraries(plate_bytetrack_ocr_sample ${PROJECT_NAME})

        install(TARGETS plate_bytetrack_ocr_sample DESTINATION bin OPTIONAL)
    endif()
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
