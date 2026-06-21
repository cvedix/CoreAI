#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"
#include <opencv2/core.hpp>

/*
 * ## 1-1-1 sample ##
 * 1 video input, 1 infer task (RF-DETR detection via TensorRT), and 1 output.
 *
 * Pipeline:
 *   file_src → rf_detr_detector (TensorRT) → osd → output
 *
 * Usage:
 *   ./1-1-1_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 *
 * Default: --mode web (open http://localhost:9091 in browser)
 */

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--log-level" && i + 1 < argc) {
            std::string level_str = argv[i + 1];
            if (level_str == "debug" || level_str == "verbose") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);
            else if (level_str == "info") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
            else if (level_str == "warning" || level_str == "warn") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
            else if (level_str == "error") CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
        }
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Limit OpenCV multithreading to a small pool to prevent CPU thread contention
    // but still allow enough parallelization for fast YOLO pre/post processing.
    cv::setNumThreads(4);

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 
        0, 
        "/home/cvedix/rapidmedia/3rdpart/core/data/video/YTDown_YouTube_Xe-o-to-di-nguoc-chieu-va-dau-nguoc-chie_Media_tPiHksyTdBU_001_1080p.mp4", 
        0.5, // Giảm resolution một nửa để tăng tốc đáng kể OSD/Web stream
        true,
        "nvh264dec",
        0,     // skip_interval
        false  // play_at_realtime = false để chạy Max Speed
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

    // create output destination based on --mode
    auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // construct pipeline
    yolo_detector_0->attach_to({file_src_0});
    bytetrack_0->attach_to({yolo_detector_0});
    osd_0->attach_to({bytetrack_0});
    output.des_node->attach_to({osd_0});

    // Flow control: Limit queue sizes to prevent startup accumulation and CPU overload
    yolo_detector_0->set_max_in_queue_size(3);
    bytetrack_0->set_max_in_queue_size(3);
    osd_0->set_max_in_queue_size(3);
    output.des_node->set_max_in_queue_size(3);

    file_src_0->start();

    sample_helper::init_board(output);
    sample_helper::print_output_info(out_cfg);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
