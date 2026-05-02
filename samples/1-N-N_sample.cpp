#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
 * ## 1-N-N sample ##
 * 1 video input and then split into 2 branches for different infer tasks
 * (YOLOv11 detection via TensorRT), then 2 total outputs (no need to sync
 * in such situations).
 *
 * Pipeline:
 *   file_src → split ─┬─→ yolov11_detector_a (TensorRT) → osd_a → output_a
 *                      └─→ yolov11_detector_b (TensorRT) → osd_b → output_b
 *
 * Usage:
 *   ./1-N-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 *
 * In web mode, branch A uses port, branch B uses port+1.
 *
 * Model path:
 *   ./cvedix_data/models/yolov11/tensorrt/yolo11n.engine  (TensorRT)
 */

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    const std::string engine_path = "./cvedix_data/models/yolov11/tensorrt/yolo11n.engine";
    const std::string labels_path = "./cvedix_data/models/yolov11/tensorrt/labels.txt";

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.25);
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", false, true);

    // branch a — YOLOv11 detector with TensorRT engine backend
    auto yolo_detector_a = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_a",
        engine_path,
        cvedix_nodes::YoloVersion::YOLO11,
        labels_path,
        0.45f, 0.5f, 0,
        cvedix_nodes::BackendType::TENSORRT
    );
    auto osd_a = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_a");
    auto output_a = sample_helper::create_output(out_cfg, "des_a", 0, {file_src_0});

    // branch b — YOLOv11 detector with TensorRT engine backend (use port+1 for web mode)
    auto out_cfg_b = out_cfg;
    out_cfg_b.web_port = out_cfg.web_port + 1;
    auto yolo_detector_b = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_b",
        engine_path,
        cvedix_nodes::YoloVersion::YOLO11,
        labels_path,
        0.45f, 0.5f, 0,
        cvedix_nodes::BackendType::TENSORRT
    );
    auto osd_b = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_b");
    auto output_b = sample_helper::create_output(out_cfg_b, "des_b", 0, {file_src_0});

    // construct pipeline
    split->attach_to({file_src_0});

    // branch a
    yolo_detector_a->attach_to({split});
    osd_a->attach_to({yolo_detector_a});
    output_a.des_node->attach_to({osd_a});

    // branch b
    yolo_detector_b->attach_to({split});
    osd_b->attach_to({yolo_detector_b});
    output_b.des_node->attach_to({osd_b});

    file_src_0->start();

    // init boards AFTER pipeline is fully attached (pipe_checker runs in board constructor)
    sample_helper::init_board(output_a);
    sample_helper::init_board(output_b);

    sample_helper::print_output_info(out_cfg);
    if (out_cfg.mode == sample_helper::OutputMode::WEB) {
        std::cout << "  Branch B: http://localhost:" << out_cfg_b.web_port << std::endl;
    }

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
