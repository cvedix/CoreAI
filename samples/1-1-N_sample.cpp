#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
 * ## 1-1-N sample ##
 * 1 video input, 1 infer task (YOLOv11 detection via TensorRT), and 1 output (switchable via --mode).
 *
 * Pipeline:
 *   file_src → yolov11_detector (TensorRT) → osd → output
 *
 * Usage:
 *   ./1-1-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 *
 * Model paths (auto-detect order):
 *   1. ./cvedix_data/models/yolov11/tensorrt/yolo11n.engine  (TensorRT)
 *   2. ./cvedix_data/models/yolov11/onnx/yolo11n.onnx        (ONNX fallback)
 */

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.8);

    // YOLOv11 detector with TensorRT engine backend
    auto yolo_detector_0 = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_0",
        "./cvedix_data/models/yolov11/tensorrt/yolo11n.engine",  // TensorRT engine model
        cvedix_nodes::YoloVersion::YOLO11,
        "./cvedix_data/models/yolov11/tensorrt/labels.txt",      // labels file
        0.45f,   // confidence threshold
        0.5f,    // NMS threshold
        0,       // class_id_offset
        cvedix_nodes::BackendType::TENSORRT
    );

    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");

    // create output destination based on --mode
    auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // construct pipeline
    yolo_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yolo_detector_0});
    output.des_node->attach_to({osd_0});

    file_src_0->start();
    sample_helper::init_board(output);
    sample_helper::print_output_info(out_cfg);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
