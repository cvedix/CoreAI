#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## N-N sample ##
* multi pipe exist separately and each pipe is 1-1-1
* (can be any structure like 1-1-N, 1-N-N)
*
* Each pipe uses YOLOv11 detection via TensorRT engine.
*
* Pipeline:
*   Pipe 0: file_src_0 → yolov11_detector_0 (TensorRT) → osd_0 → output_0
*   Pipe 1: file_src_1 → yolov11_detector_1 (TensorRT) → osd_1 → output_1
*
* Usage:
*   ./N-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*
* In web mode, pipe 0 uses port, pipe 1 uses port+1.
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    const std::string engine_path = "./cvedix_data/models/yolov11/tensorrt/yolo11n.engine";
    const std::string labels_path = "./cvedix_data/models/yolov11/tensorrt/labels.txt";

    // create nodes - pipe 0
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.6);
    auto yolo_detector_0 = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_0",
        engine_path,
        cvedix_nodes::YoloVersion::YOLO11,
        labels_path,
        0.45f, 0.5f, 0,
        cvedix_nodes::BackendType::TENSORRT
    );
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto output_0 = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // create nodes - pipe 1 (port+1 for web mode)
    auto out_cfg_1 = out_cfg;
    out_cfg_1.web_port = out_cfg.web_port + 1;
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 0, "./cvedix_data/video/face2.mp4");
    auto yolo_detector_1 = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_1",
        engine_path,
        cvedix_nodes::YoloVersion::YOLO11,
        labels_path,
        0.45f, 0.5f, 0,
        cvedix_nodes::BackendType::TENSORRT
    );
    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_1");
    auto output_1 = sample_helper::create_output(out_cfg_1, "des_1", 0, {file_src_1});

    // construct pipeline
    // pipe 0
    yolo_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yolo_detector_0});
    output_0.des_node->attach_to({osd_0});

    // pipe 1
    yolo_detector_1->attach_to({file_src_1});
    osd_1->attach_to({yolo_detector_1});
    output_1.des_node->attach_to({osd_1});

    file_src_0->start();
    file_src_1->start();

    sample_helper::init_board(output_0);
    sample_helper::init_board(output_1);

    sample_helper::print_output_info(out_cfg);
    if (out_cfg.mode == sample_helper::OutputMode::WEB) {
        std::cout << "  Pipe 1: http://localhost:" << out_cfg_1.web_port << std::endl;
    }

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    file_src_1->detach_recursively();
}
