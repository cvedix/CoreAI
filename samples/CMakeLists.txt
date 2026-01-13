# can ONLY be called by parent CMakeLists.txt, because no environment settings for this script.#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "can ONLY be called by parent CMakeLists.txt.")
endif()

message("start build for simple samples...")
# save all exe to 'build/bin'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(1-1-1_sample "1-1-1_sample.cpp")
target_link_libraries(1-1-1_sample ${PROJECT_NAME})

add_executable(1-1-N_sample "1-1-N_sample.cpp")
target_link_libraries(1-1-N_sample ${PROJECT_NAME})

add_executable(1-N-N_sample "1-N-N_sample.cpp")
target_link_libraries(1-N-N_sample ${PROJECT_NAME})

add_executable(ba_crossline_sample "ba_crossline_sample.cpp")
target_link_libraries(ba_crossline_sample ${PROJECT_NAME})

add_executable(ba_multiline_crossline_test "ba_multiline_crossline_test.cpp")
target_link_libraries(ba_multiline_crossline_test ${PROJECT_NAME})

add_executable(wrong_way_detection_sample "wrong_way_detection_sample.cpp")
target_link_libraries(wrong_way_detection_sample ${PROJECT_NAME})

add_executable(ba_multiple_crossline_counting_sample "ba_multiple_crossline_counting_sample.cpp")
target_link_libraries(ba_multiple_crossline_counting_sample ${PROJECT_NAME})

add_executable(enet_seg_sample "enet_seg_sample.cpp")
target_link_libraries(enet_seg_sample ${PROJECT_NAME})

add_executable(face_tracking_sample "face_tracking_sample.cpp")
target_link_libraries(face_tracking_sample ${PROJECT_NAME})

add_executable(face_tracking_bytetrack_sample "face_tracking_bytetrack_sample.cpp")
target_link_libraries(face_tracking_bytetrack_sample ${PROJECT_NAME})

add_executable(face_tracking_ocsort_sample "face_tracking_ocsort_sample.cpp")
target_link_libraries(face_tracking_ocsort_sample ${PROJECT_NAME})

# InsightFace ONNX samples (no TensorRT required)
# add_executable(insightface_sample "insightface_sample.cpp")
# target_link_libraries(insightface_sample ${PROJECT_NAME})

# add_executable(insightface_register_recognize_face_sample "insightface_register_recognize_face_sample.cpp")
# target_link_libraries(insightface_register_recognize_face_sample ${PROJECT_NAME})

# Face registration sample with augmentation support
# add_executable(face_registration_sample "face_registration_sample.cpp")
# target_link_libraries(face_registration_sample ${PROJECT_NAME})

# Face recognition sample (uses database from registration)
add_executable(face_recognition_sample "face_recognition_sample.cpp")
target_link_libraries(face_recognition_sample ${PROJECT_NAME})

# Face recognition with ONNX Runtime (supports glintr100 and other InsightFace models)
set(ONNXRUNTIME_DIR ${CMAKE_SOURCE_DIR}/third_party/onnxruntime)
if(EXISTS ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)
    add_executable(face_recognition_ort_sample "face_recognition_ort_sample.cpp")
    target_include_directories(face_recognition_ort_sample PRIVATE ${ONNXRUNTIME_DIR}/include)
    target_link_libraries(face_recognition_ort_sample 
        ${PROJECT_NAME}
        ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so
    )
    # Set runtime path for finding libonnxruntime.so
    set_target_properties(face_recognition_ort_sample PROPERTIES
        BUILD_RPATH ${ONNXRUNTIME_DIR}/lib
        INSTALL_RPATH ${ONNXRUNTIME_DIR}/lib
    )
    message(STATUS "Building face_recognition_ort_sample with ONNX Runtime")
else()
    message(STATUS "ONNX Runtime not found at ${ONNXRUNTIME_DIR}, skipping face_recognition_ort_sample")
endif()

# InsightFace TensorRT samples (requires CVEDIX_WITH_TRT=ON)
if(CVEDIX_WITH_TRT)
    # add_executable(insightface_trt_sample "insightface_trt_sample.cpp")
    # target_include_directories(insightface_trt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    # target_link_libraries(insightface_trt_sample ${PROJECT_NAME})
    
    # Note: insightface_trt_simple_sample.cpp does not exist, commented out
    # add_executable(insightface_trt_simple_sample "insightface_trt_simple_sample.cpp")
    # target_link_libraries(insightface_trt_simple_sample ${PROJECT_NAME})
    
    # Use correct filename: insightface_register_recognize_face_trt_sample.cpp
    # add_executable(insightface_register_recognize_face_trt_sample "insightface_register_recognize_face_trt_sample.cpp")
    # target_include_directories(insightface_register_recognize_face_trt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    # target_link_libraries(insightface_register_recognize_face_trt_sample ${PROJECT_NAME})
endif()

add_executable(face_tracking_rtsp_sample "face_tracking_rtsp_sample.cpp")
target_include_directories(face_tracking_rtsp_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(face_tracking_rtsp_sample ${PROJECT_NAME})

add_executable(image_des_sample "image_des_sample.cpp")
target_link_libraries(image_des_sample ${PROJECT_NAME})

add_executable(image_src_sample "image_src_sample.cpp")
target_link_libraries(image_src_sample ${PROJECT_NAME})

add_executable(interaction_with_pipe_sample "interaction_with_pipe_sample.cpp")
target_link_libraries(interaction_with_pipe_sample ${PROJECT_NAME})

add_executable(mask_rcnn_sample "mask_rcnn_sample.cpp")
target_link_libraries(mask_rcnn_sample ${PROJECT_NAME})

add_executable(message_broker_sample "message_broker_sample.cpp")
target_link_libraries(message_broker_sample ${PROJECT_NAME})

add_executable(sse_broker_sample "sse_broker_sample.cpp")
target_link_libraries(sse_broker_sample ${PROJECT_NAME})

# add_executable(face_recognition_mqtt_sample "face_recognition_mqtt_sample.cpp")
# target_link_libraries(face_recognition_mqtt_sample ${PROJECT_NAME})


add_executable(multi_detectors_and_classifiers_sample "multi_detectors_and_classifiers_sample.cpp")
target_link_libraries(multi_detectors_and_classifiers_sample ${PROJECT_NAME})

add_executable(N-1-N_sample "N-1-N_sample.cpp")
target_link_libraries(N-1-N_sample ${PROJECT_NAME})

add_executable(N-N_sample "N-N_sample.cpp")
target_link_libraries(N-N_sample ${PROJECT_NAME})

add_executable(openpose_sample "openpose_sample.cpp")
target_link_libraries(openpose_sample ${PROJECT_NAME})

add_executable(record_sample "record_sample.cpp")
target_link_libraries(record_sample ${PROJECT_NAME})

# RTSP samples
add_executable(rtsp_des_sample "rtsp_des_sample.cpp")
target_link_libraries(rtsp_des_sample ${PROJECT_NAME})

add_executable(rtsp_src_sample "rtsp_src_sample.cpp")
target_link_libraries(rtsp_src_sample ${PROJECT_NAME})

add_executable(rtsp_ba_crossline_sample "rtsp_ba_crossline_sample.cpp")
target_link_libraries(rtsp_ba_crossline_sample ${PROJECT_NAME})

add_executable(cvedix_logger_sample "cvedix_logger_sample.cpp")
target_link_libraries(cvedix_logger_sample ${PROJECT_NAME})

# License check sample (only build if license support is enabled)
if(CVEDIX_WITH_LICENSE)
    add_executable(license_check_sample "license_check_sample.cpp")
    target_link_libraries(license_check_sample PUBLIC ${PROJECT_NAME})
    # Ensure licensecxx include directories are available
    if(TARGET lcxx::lcxx)
        target_link_libraries(license_check_sample PUBLIC lcxx::lcxx)
    endif()
    
    # License info sample - demonstrates cvedix_license_node usage
    add_executable(license_info_sample "license_info_sample.cpp")
    target_link_libraries(license_info_sample PUBLIC ${PROJECT_NAME})
    if(TARGET lcxx::lcxx)
        target_link_libraries(license_info_sample PUBLIC lcxx::lcxx)
    endif()
endif()

add_executable(skip_sample "skip_sample.cpp")
target_link_libraries(skip_sample ${PROJECT_NAME})

add_executable(obstacle_detect_sample "obstacle_detect_sample.cpp")
target_link_libraries(obstacle_detect_sample ${PROJECT_NAME})

add_executable(multi_detectors_sample "multi_detectors_sample.cpp")
target_link_libraries(multi_detectors_sample ${PROJECT_NAME})

add_executable(firesmoke_detect_sample "firesmoke_detect_sample.cpp")
target_link_libraries(firesmoke_detect_sample ${PROJECT_NAME})

add_executable(yolov11_onnx_detector_sample "yolov11_onnx_detector_sample.cpp")
target_link_libraries(yolov11_onnx_detector_sample ${PROJECT_NAME})

add_executable(yolov11_plate_detector_sample "yolov11_plate_detector_sample.cpp")
target_link_libraries(yolov11_plate_detector_sample ${PROJECT_NAME})

add_executable(face_swap_sample "face_swap_sample.cpp")
target_link_libraries(face_swap_sample ${PROJECT_NAME})

add_executable(face_yunet_int8_sample "face_yunet_int8_sample.cpp")
target_link_libraries(face_yunet_int8_sample ${PROJECT_NAME})

add_executable(video_restoration_sample "video_restoration_sample.cpp")
target_link_libraries(video_restoration_sample ${PROJECT_NAME})

add_executable(app_des_sample "app_des_sample.cpp")
target_link_libraries(app_des_sample ${PROJECT_NAME})

add_executable(app_src_des_sample "app_src_des_sample.cpp")
target_link_libraries(app_src_des_sample ${PROJECT_NAME})

add_executable(lane_detect_sample "lane_detect_sample.cpp")
target_link_libraries(lane_detect_sample ${PROJECT_NAME})

add_executable(frame_fusion_sample "frame_fusion_sample.cpp")
target_link_libraries(frame_fusion_sample ${PROJECT_NAME})

add_executable(cvedix_test "cvedix_test.cpp")
target_link_libraries(cvedix_test ${PROJECT_NAME})

# Install all basic samples
install(TARGETS 
    1-1-1_sample 1-1-N_sample 1-N-N_sample
    ba_crossline_sample enet_seg_sample face_tracking_sample
    image_des_sample image_src_sample interaction_with_pipe_sample
    mask_rcnn_sample message_broker_sample multi_detectors_and_classifiers_sample
    N-1-N_sample N-N_sample openpose_sample record_sample
    cvedix_logger_sample skip_sample obstacle_detect_sample
    multi_detectors_sample firesmoke_detect_sample face_swap_sample
    face_yunet_int8_sample yolov11_onnx_detector_sample yolov11_plate_detector_sample
    video_restoration_sample app_des_sample app_src_des_sample
    lane_detect_sample frame_fusion_sample cvedix_test
    # insightface_sample insightface_register_recognize_face_sample
    DESTINATION bin
    OPTIONAL
)

# Install RTSP samples
install(TARGETS 
    rtsp_des_sample
    rtsp_src_sample
    rtsp_ba_crossline_sample
    DESTINATION bin
    OPTIONAL
)

# samples depend on Kafka
if(CVEDIX_WITH_KAFKA)
    add_executable(message_broker_kafka_sample "message_broker_kafka_sample.cpp")
    target_link_libraries(message_broker_kafka_sample ${PROJECT_NAME})
    
    install(TARGETS message_broker_kafka_sample
            DESTINATION bin
            OPTIONAL)
endif()

# samples depend on MQTT
if(CVEDIX_WITH_MQTT)
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
endif()

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

# samples depend on FFmpeg
if(CVEDIX_WITH_FFMPEG)
    add_executable(ffmpeg_src_des_sample "ffmpeg_src_des_sample.cpp")
    target_link_libraries(ffmpeg_src_des_sample ${PROJECT_NAME})

    add_executable(ffmpeg_transcode_sample "ffmpeg_transcode_sample.cpp")
    target_link_libraries(ffmpeg_transcode_sample ${PROJECT_NAME})
    
    install(TARGETS 
        ffmpeg_src_des_sample
        ffmpeg_transcode_sample
        DESTINATION bin
        OPTIONAL)
endif()

# samples depend on RKNN
if(CVEDIX_WITH_RKNN)
    add_executable(rknn_yolov11_detector_sample "rknn_yolov11_detector_sample.cpp")
    target_link_libraries(rknn_yolov11_detector_sample ${PROJECT_NAME})

    add_executable(rknn_face_detector_sample "rknn_face_detector_sample.cpp")
    target_link_libraries(rknn_face_detector_sample ${PROJECT_NAME})

    add_executable(rknn_yolov8_face_detection_simple_sample "rknn_yolov8_face_detection_simple_sample.cpp")
    target_link_libraries(rknn_yolov8_face_detection_simple_sample ${PROJECT_NAME})

    add_executable(rknn_yolov8_face_rtsp_sample "rknn_yolov8_face_rtsp_sample.cpp")
    target_link_libraries(rknn_yolov8_face_rtsp_sample ${PROJECT_NAME})

    add_executable(rknn_face_detection_sample "rknn_face_detection_sample.cpp")
    target_link_libraries(rknn_face_detection_sample ${PROJECT_NAME})

    add_executable(rknn_face_detection_file_sample "rknn_face_detection_file_sample.cpp")
    target_link_libraries(rknn_face_detection_file_sample ${PROJECT_NAME})

    add_executable(rknn_rtsp_tracking_sample "rknn_rtsp_tracking_sample.cpp")
    target_link_libraries(rknn_rtsp_tracking_sample ${PROJECT_NAME})

    add_executable(rknn_rtsp_tracking_mqtt_sample "rknn_rtsp_tracking_mqtt_sample.cpp")
    target_include_directories(rknn_rtsp_tracking_mqtt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${PROJECT_NAME})
    
    # Link with mosquitto if MQTT is enabled
    if(CVEDIX_WITH_MQTT)
        if(MOSQUITTO_FOUND)
            target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${MOSQUITTO_LIBRARIES})
        else()
            find_library(MOSQUITTO_LIB NAMES mosquitto)
            if(MOSQUITTO_LIB)
                target_link_libraries(rknn_rtsp_tracking_mqtt_sample ${MOSQUITTO_LIB})
            endif()
        endif()
    endif()

    # RKNN face tracking sample - build by default when RKNN is enabled and MQTT is enabled
    if(CVEDIX_WITH_MQTT)
        add_executable(rknn_face_tracking_sample "rknn_face_tracking_sample.cpp")
        target_include_directories(rknn_face_tracking_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
        target_link_libraries(rknn_face_tracking_sample ${PROJECT_NAME})
        
        # Link with mosquitto for MQTT
        if(MOSQUITTO_FOUND)
            target_link_libraries(rknn_face_tracking_sample ${MOSQUITTO_LIBRARIES})
        else()
            find_library(MOSQUITTO_LIB NAMES mosquitto)
            if(MOSQUITTO_LIB)
                target_link_libraries(rknn_face_tracking_sample ${MOSQUITTO_LIB})
            endif()
        endif()
        
        # Install RKNN face tracking sample
        install(TARGETS rknn_face_tracking_sample DESTINATION bin)
    endif()

    # Install RKNN samples
    install(TARGETS 
        rknn_yolov11_detector_sample
        rknn_face_detector_sample
        rknn_yolov8_face_detection_simple_sample
        rknn_yolov8_face_rtsp_sample
        rknn_face_detection_sample
        rknn_face_detection_file_sample
        rknn_rtsp_tracking_sample
        DESTINATION bin
    )
    
    # Install RKNN MQTT sample
    install(TARGETS 
        rknn_rtsp_tracking_mqtt_sample
        DESTINATION bin
        OPTIONAL
    )
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
if(CVEDIX_WITH_TRT)
    # TensorRT YOLOv11 Plate Detector sample
    add_executable(yolov11_plate_detector_trt_sample "yolov11_plate_detector_trt_sample.cpp")
    target_include_directories(yolov11_plate_detector_trt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_plate_detector_trt_sample ${PROJECT_NAME})
    
    # TensorRT YOLOv11 Face Detector sample
    add_executable(yolov11_face_detector_trt_sample "yolov11_face_detector_trt_sample.cpp")
    target_include_directories(yolov11_face_detector_trt_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_face_detector_trt_sample ${PROJECT_NAME})

    # TensorRT YOLOv11 Face Detector with Video Output
    add_executable(yolov11_face_detector_video_output_sample "yolov11_face_detector_video_output_sample.cpp")
    target_include_directories(yolov11_face_detector_video_output_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_face_detector_video_output_sample ${PROJECT_NAME})

    # YOLOv11 Face Detection + ByteTrack Tracking sample
    add_executable(yolov11_face_bytetrack_sample "yolov11_face_bytetrack_sample.cpp")
    target_include_directories(yolov11_face_bytetrack_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_face_bytetrack_sample ${PROJECT_NAME})

    # YOLOv11 Plate Detection + ByteTrack Tracking sample with Analysis Board
    add_executable(yolov11_plate_bytetrack_sample "yolov11_plate_bytetrack_sample.cpp")
    target_include_directories(yolov11_plate_bytetrack_sample PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(yolov11_plate_bytetrack_sample ${PROJECT_NAME})

    
    # NOTE: Following samples require trt_vehicle which is temporarily disabled
    # Uncomment when trt_vehicle is updated to TensorRT 10.x
    
    # add_executable(ba_jam_sample "ba_jam_sample.cpp")
    # target_link_libraries(ba_jam_sample ${PROJECT_NAME})

    # add_executable(ba_stop_sample "ba_stop_sample.cpp")
    # target_link_libraries(ba_stop_sample ${PROJECT_NAME})

    # add_executable(body_scan_and_plate_detect_sample "body_scan_and_plate_detect_sample.cpp")
    # target_link_libraries(body_scan_and_plate_detect_sample ${PROJECT_NAME})

    # add_executable(dynamic_pipeline_sample "dynamic_pipeline_sample.cpp")      # DISABLED - needs trt_vehicle
    # target_link_libraries(dynamic_pipeline_sample ${PROJECT_NAME})

    # add_executable(dynamic_pipeline_sample2 "dynamic_pipeline_sample2.cpp")    # DISABLED - needs trt_vehicle
    # target_link_libraries(dynamic_pipeline_sample2 ${PROJECT_NAME})    

    # add_executable(message_broker_sample2 "message_broker_sample2.cpp")       # DISABLED - needs trt_vehicle
    # target_link_libraries(message_broker_sample2 ${PROJECT_NAME})

    # add_executable(multi_trt_infer_nodes_sample "multi_trt_infer_nodes_sample.cpp")  # DISABLED - needs trt_vehicle/trt_yolov8
    # target_link_libraries(multi_trt_infer_nodes_sample ${PROJECT_NAME})

    # add_executable(plate_recognize_sample "plate_recognize_sample.cpp")       # DISABLED - needs trt_vehicle
    # target_link_libraries(plate_recognize_sample ${PROJECT_NAME})

    # add_executable(src_des_sample "src_des_sample.cpp")
    # target_link_libraries(src_des_sample ${PROJECT_NAME})

    # add_executable(trt_infer_sample "trt_infer_sample.cpp")
    # target_link_libraries(trt_infer_sample ${PROJECT_NAME})

    # add_executable(vehicle_body_scan_sample "vehicle_body_scan_sample.cpp")
    # target_link_libraries(vehicle_body_scan_sample ${PROJECT_NAME})

    # add_executable(vehicle_cluster_based_on_classify_encoding_sample "vehicle_cluster_based_on_classify_encoding_sample.cpp")
    # target_link_libraries(vehicle_cluster_based_on_classify_encoding_sample ${PROJECT_NAME})

    # add_executable(vehicle_tracking_sample "vehicle_tracking_sample.cpp")
    # target_link_libraries(vehicle_tracking_sample ${PROJECT_NAME})

    # add_executable(1-N-1_sample "1-N-1_sample.cpp")                            # DISABLED - needs trt_vehicle
    # target_link_libraries(1-N-1_sample ${PROJECT_NAME})

    # add_executable(1-N-1_sample2 "1-N-1_sample2.cpp")                          # DISABLED - needs trt_vehicle
    # target_link_libraries(1-N-1_sample2 ${PROJECT_NAME})

    # add_executable(1-N-1_sample3 "1-N-1_sample3.cpp")                          # DISABLED - needs trt_vehicle
    # target_link_libraries(1-N-1_sample3 ${PROJECT_NAME})

    # add_executable(trt_yolov8_sample "trt_yolov8_sample.cpp")                  # DISABLED - needs trt_yolov8
    # target_link_libraries(trt_yolov8_sample ${PROJECT_NAME})

    # add_executable(trt_yolov8_sample2 "trt_yolov8_sample2.cpp")                # DISABLED - needs trt_yolov8
    # target_link_libraries(trt_yolov8_sample2 ${PROJECT_NAME})

    # add_executable(rtmp_src_sample "rtmp_src_sample.cpp")                      # DISABLED - needs trt_vehicle
    # target_link_libraries(rtmp_src_sample ${PROJECT_NAME})
    
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