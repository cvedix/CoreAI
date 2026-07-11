#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include <opencv2/core.hpp>
#include <iostream>
#include <string>

/*
 * ## 1-1-1 sample ##
 * 1 video input, 1 infer task (YOLOv12 detection via TensorRT), and 1 RTMP output.
 *
 * Pipeline:
 *   file_src -> yolo_detector (TensorRT) -> bytetrack -> osd -> rtmp
 *
 * Usage:
 *   ./1-1-1_sample [--rtmp rtmp://console.vinguard.cloud:1935/live/9000]
 *
 * Default: push RTMP stream to console.vinguard.cloud:1935.
 */

int main(int argc, char** argv) {
    std::string rtmp_url = "rtmp://console.vinguard.cloud:1935/live/9000";

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--log-level" && i + 1 < argc) {
            std::string level_str = argv[i + 1];
            if (level_str == "debug" || level_str == "verbose") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);
            else if (level_str == "info") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
            else if (level_str == "warning" || level_str == "warn") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
            else if (level_str == "error") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
            ++i;
        } else if ((std::string(argv[i]) == "--rtmp" || std::string(argv[i]) == "--rtmp-url") && i + 1 < argc) {
            rtmp_url = argv[++i];
        }
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Keep enough OpenCV workers for detector preprocess/OSD. Source resizing is
    // handled in the CUDA GStreamer pipeline below, not by cv::resize.
    cv::setNumThreads(8);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 
        0, 
        "/home/cvedix/rapidmedia/3rdpart/CoreAI/data/video/YTDown_YouTube_Xe-o-to-di-nguoc-chieu-va-dau-nguoc-chie_Media_tPiHksyTdBU_001_1080p.mp4",
        1.0, // Resize đã được thực hiện trong GStreamer CUDA pipeline bên dưới
        true,
        "nvh264dec ! cudaconvert ! video/x-raw(memory:CUDAMemory),format=BGRx ! "
        "cudascale ! video/x-raw(memory:CUDAMemory),format=BGRx,width=960,height=540 ! "
        "cudadownload ! video/x-raw,format=BGRx ! videoconvert ! video/x-raw,format=BGR",
        3,     // skip_interval: ~7.5 FPS input from 30 FPS video, avoids detector queue flooding
        true   // play_at_realtime = true để source không flood detector queue
    );

    // YOLOv12 detector with TensorRT engine backend
    auto yolo_detector_0 = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_0",
        "/home/cvedix/cvedix_data/yolo12n.engine",  // TensorRT engine model
        cvedix_nodes::YoloVersion::YOLO12,
        "/home/cvedix/cvedix_data/coco.txt",      // labels file
        0.30f,   // confidence threshold
        0.5f,    // NMS threshold
        0,       // class_id_offset
        cvedix_nodes::BackendType::TENSORRT
    );

    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id_in_label = false;
    osd_cfg.bbox_color = {0, 255, 0}; // green
    osd_0->update_config(osd_cfg);

    auto bytetrack_0 = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "bytetrack_0",
        cvedix_nodes::cvedix_track_for::NORMAL,
        0.5f,  // track_thresh
        0.6f,  // high_thresh
        0.8f,  // match_thresh
        30,    // track_buffer
        30     // fps
    );

    auto rtmp_output_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "des_0_rtmp",
        0,
        rtmp_url,
        cvedix_objects::cvedix_size{}, // giữ nguyên resolution từ OSD/source
        2048,                          // kbps, phù hợp luồng 960x540
        true,                          // đẩy frame đã vẽ OSD
        "nvh264enc",                   // dùng NVENC thay vì x264 CPU
        false                          // dùng đúng stream URL, không tự thêm _0
    );

    // construct pipeline
    yolo_detector_0->attach_to({file_src_0});
    bytetrack_0->attach_to({yolo_detector_0});
    osd_0->attach_to({bytetrack_0});
    rtmp_output_0->attach_to({osd_0});

    // Flow control: Limit queue sizes to prevent startup accumulation and CPU overload
    yolo_detector_0->set_max_in_queue_size(1);
    bytetrack_0->set_max_in_queue_size(1);
    osd_0->set_max_in_queue_size(1);
    rtmp_output_0->set_max_in_queue_size(1);

    file_src_0->start();

    std::cout << "\nRTMP output: " << rtmp_url
              << "\nPress Enter to stop...\n\n";

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
