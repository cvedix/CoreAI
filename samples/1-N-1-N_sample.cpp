/**
 * @file 1-N-1-N_sample.cpp
 * @brief 1 input → N parallel detectors → 1 sync (merge) → N outputs
 *
 * Demonstrates running 2 different YOLOv11 TensorRT models in parallel on the
 * same video stream, then merging all detection results back into a single
 * frame_meta before fanning out to multiple outputs.
 *
 * Pipeline:
 *
 *                        ┌─→ face_detector  (YOLOv11-face TRT) ──┐
 *   file_src → split ───┤                                        ├─→ sync (MERGE) → split → osd_0 → output_0
 *                        └─→ vehicle_detector (YOLOv11n TRT) ────┘               ↘ → osd_1 → output_1
 *
 * Branch A: YOLOv11 face detection   (class 0 = face)
 * Branch B: YOLOv11n COCO detection  (classes 2,3,5,7 = car,motorcycle,bus,truck)
 *
 * The sync node (MERGE mode) combines target collections from both branches
 * by matching frame_index, so the merged frame_meta contains all detections.
 *
 * Usage:
 *   ./1-N-1-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 *
 * In web mode, output_0 uses port, output_1 uses port+1.
 */

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_sync_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

#include <fstream>

namespace {

/// @brief Create a temporary labels file with a single label
std::string create_temp_labels(const std::string& filename, const std::vector<std::string>& labels) {
    const std::string path = "/tmp/" + filename;
    std::ofstream out(path, std::ios::trunc);
    for (const auto& label : labels) {
        out << label << "\n";
    }
    return path;
}

}  // namespace

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // ══════════════════════════════════════════════════════
    // Model paths
    // ══════════════════════════════════════════════════════
    const std::string face_engine   = "./cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine";
    const std::string vehicle_engine = "./cvedix_data/models/yolo11n.onnx";
    const std::string vehicle_labels = "./cvedix_data/models/yolov11/tensorrt/labels.txt";

    // Face model has 1 class (face) — create a temp labels file
    const std::string face_labels = create_temp_labels("cvedix_face_labels.txt", {"face"});

    // ══════════════════════════════════════════════════════
    // 1. Source
    // ══════════════════════════════════════════════════════
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        0.5,     // resize_ratio (50% of original)
        true,    // cycle
        "avdec_h264",
        5        // skip_interval: process every 6th frame (~4 fps input for ONNX)
    );

    // ══════════════════════════════════════════════════════
    // 2. Split: deep copy so each branch gets its own frame_meta
    // ══════════════════════════════════════════════════════
    auto split_input = std::make_shared<cvedix_nodes::cvedix_split_node>(
        "split_input", false, true);  // deep_copy = true

    // ══════════════════════════════════════════════════════
    // 3. Branch A: Face Detection (YOLOv11-face TensorRT)
    // ══════════════════════════════════════════════════════
    auto face_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "face_detector",
        face_engine,
        cvedix_nodes::YoloVersion::YOLO11,
        face_labels,
        0.35f,   // lower threshold — faces can be small
        0.45f,
        0,       // class_id_offset = 0 (face = class 0)
        cvedix_nodes::BackendType::AUTO
    );
    face_detector->set_allowed_classes({0});  // only face

    // ══════════════════════════════════════════════════════
    // 4. Branch B: Vehicle Detection (YOLOv11n COCO TensorRT)
    // ══════════════════════════════════════════════════════
    auto vehicle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "vehicle_detector",
        vehicle_engine,
        cvedix_nodes::YoloVersion::YOLO11,
        vehicle_labels,
        0.45f,
        0.5f,
        100,     // class_id_offset = 100 to avoid collision with face class IDs
        cvedix_nodes::BackendType::ONNX
    );
    // COCO: car(2), motorcycle(3), bus(5), truck(7)
    // After offset +100: 102, 103, 105, 107
    // NOTE: set_allowed_classes checks AFTER class_id_offset is applied
    vehicle_detector->set_allowed_classes({102, 103, 105, 107});

    // ══════════════════════════════════════════════════════
    // 5. Sync: MERGE mode — combine targets from both branches
    // ══════════════════════════════════════════════════════
    auto sync = std::make_shared<cvedix_nodes::cvedix_sync_node>(
        "sync_merge",
        cvedix_nodes::cvedix_sync_mode::MERGE,
        500  // timeout ms (wait for slower ONNX branch)
    );

    // ══════════════════════════════════════════════════════
    // 6. Split output: fan out to N outputs
    // ══════════════════════════════════════════════════════
    auto split_output = std::make_shared<cvedix_nodes::cvedix_split_node>(
        "split_output", false, true);  // deep_copy for thread safety

    // ══════════════════════════════════════════════════════
    // 7. OSD + Outputs
    // ══════════════════════════════════════════════════════
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto output_0 = sample_helper::create_output(out_cfg, "des_0", 0, {file_src});

    auto out_cfg_1 = out_cfg;
    out_cfg_1.web_port = out_cfg.web_port + 1;
    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_1");
    auto output_1 = sample_helper::create_output(out_cfg_1, "des_1", 0, {file_src});

    // ══════════════════════════════════════════════════════
    // Build Pipeline
    // ══════════════════════════════════════════════════════
    //
    //                       ┌─→ face_detector ──────┐
    //  file_src → split ───┤                         ├─→ sync → split → osd_0 → output_0
    //                       └─→ vehicle_detector ───┘                 → osd_1 → output_1
    //

    // Input split
    split_input->attach_to({file_src});

    // Branch A: face
    face_detector->attach_to({split_input});

    // Branch B: vehicle
    vehicle_detector->attach_to({split_input});

    // Sync/Merge
    sync->attach_to({face_detector, vehicle_detector});

    // Output split
    split_output->attach_to({sync});

    // Output branches
    osd_0->attach_to({split_output});
    output_0.des_node->attach_to({osd_0});

    osd_1->attach_to({split_output});
    output_1.des_node->attach_to({osd_1});

    // ══════════════════════════════════════════════════════
    // Start
    // ══════════════════════════════════════════════════════
    file_src->start();

    sample_helper::init_board(output_0);
    sample_helper::init_board(output_1);

    sample_helper::print_output_info(out_cfg);
    std::cout << "  Pipeline: file_src → split → [face_detector + vehicle_detector] → sync → split → 2 outputs" << std::endl;
    if (out_cfg.mode == sample_helper::OutputMode::WEB) {
        std::cout << "  Output 1: http://localhost:" << out_cfg_1.web_port << std::endl;
    }

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
}
