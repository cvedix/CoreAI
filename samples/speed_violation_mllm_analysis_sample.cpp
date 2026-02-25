/**
 * @file speed_violation_mllm_analysis_sample.cpp
 * @brief Sample: MLLM phân tích đặc trưng phương tiện khi vượt quá tốc độ 120km/h
 *
 * Pipeline:
 *   file_src → trt_detector → tracker → speed_estimation → mllm_trigger → osd → file_des
 *
 * Khi phương tiện vượt tốc độ 120km/h:
 *   1. speed_estimation node emit BA result SPEED [VIOLATION]
 *   2. mllm trigger nhận BA result, crop xe vi phạm
 *   3. Gửi ảnh crop xe đến minicpm-v (Ollama) để phân tích đặc trưng
 *   4. Kết quả lưu vào frame_meta->description + log file + crop image
 *
 * NOTE: MLLM chỉ được gọi khi có violation, không phải mọi frame!
 *
 * Requires:
 *   -DCVEDIX_WITH_TRT=ON
 *   -DCVEDIX_WITH_LLM=ON
 *   Ollama running with minicpm-v model
 */

#if defined(CVEDIX_WITH_TRT) && defined(CVEDIX_WITH_LLM)

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_detector_node.h"
#include "cvedix/nodes/infers/cvedix_speed_violation_mllm_trigger_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_speed_estimation_node.h"
#include "cvedix/nodes/osd/cvedix_ba_line_crossline_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::experimental::filesystem::create_directories("./output");

    // === 1. Source: video file ===
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0,
        "./cvedix_data/test_video/vehicle_count.mp4",
        0.6, false
    );

    // === 2. Detector: TensorRT YOLOv11 ===
    auto detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_detector_node>(
        "detector",
        "./cvedix_data/models/yolov11n.engine",
        "./cvedix_data/models/coco_80_labels_list.txt",
        0.15f, 0.45f
    );
    detector->set_allowed_classes({2, 3, 5, 7}); // car, motorbike, bus, truck

    // === 3. Tracker: SORT (OpenCV KalmanFilter, no Eigen conflict) ===
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "tracker",
        cvedix_nodes::cvedix_track_for::NORMAL
    );

    // === 4. Speed Estimation ===
    cvedix_objects::cvedix_line line1(
        cvedix_objects::cvedix_point(120, 290),
        cvedix_objects::cvedix_point(570, 290));
    cvedix_objects::cvedix_line line2(
        cvedix_objects::cvedix_point(50, 460),
        cvedix_objects::cvedix_point(640, 460));

    auto speed_est = std::make_shared<cvedix_nodes::cvedix_ba_line_speed_estimation_node>(
        "speed_estimation",
        std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>>{{0, {line1, line2}}},
        std::map<int, double>{{0, 50.0 / 170.0}},
        120.0,   // speed limit: 120 km/h
        true,    // record image on violation
        false
    );

    // === 5. MLLM Trigger: chỉ phân tích KHI có violation ===
    std::string vehicle_prompt =
        "Bạn là AI hỗ trợ xử phạt vi phạm giao thông.\n"
        "Hãy phân tích phương tiện vi phạm tốc độ trong ảnh và mô tả:\n"
        "1. Loại phương tiện (sedan, SUV, bán tải, xe tải, xe buýt, xe máy...)\n"
        "2. Màu sắc chính của xe\n"
        "3. Đặc điểm nhận dạng (logo hãng xe, kiểu dáng đặc biệt)\n"
        "4. Biển số xe (nếu nhìn thấy được)\n"
        "5. Hướng di chuyển\n"
        "Trả lời ngắn gọn, mỗi mục 1 dòng.";

    auto mllm_trigger = std::make_shared<cvedix_nodes::cvedix_speed_violation_mllm_trigger_node>(
        "mllm_trigger",
        "minicpm-v",                       // Ollama model
        vehicle_prompt,
        "http://localhost:11434",          // Ollama URL
        "./output/speed_violations.log",   // Log file
        300                                // Cooldown: 300 frames (~10s at 30fps)
    );

    // === 6. OSD + File output ===
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_osd_node>("osd");
    auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
        "file_out", 0, "./output", "speed_mllm_", 10,
        cvedix_objects::cvedix_size(), 2048, true);

    // === Build pipeline ===
    //
    //  file_src → detector → tracker → speed_estimation → mllm_trigger → osd → file_des
    //
    //  mllm_trigger: passthrough nếu không có violation
    //                crop + gọi MLLM nếu speed > 120km/h
    //
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    speed_est->attach_to({tracker});
    mllm_trigger->attach_to({speed_est});
    osd->attach_to({mllm_trigger});
    file_des->attach_to({osd});

    // === Start ===
    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  Speed Violation + MLLM Vehicle Analysis Sample");
    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  Speed limit: 120 km/h");
    CVEDIX_INFO("  MLLM model: minicpm-v (Ollama @ localhost:11434)");
    CVEDIX_INFO("  On violation (>120 km/h):");
    CVEDIX_INFO("    - Crop violating vehicle");
    CVEDIX_INFO("    - Analyze with MLLM (type, color, plate...)");
    CVEDIX_INFO("    - Log to ./output/speed_violations.log");
    CVEDIX_INFO("    - Save crop to ./output/violation_*.jpg");
    CVEDIX_INFO("  Output video: ./output/speed_mllm_*.mp4");
    CVEDIX_INFO("  Press ENTER to stop...");
    CVEDIX_INFO("================================================================");

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);

    CVEDIX_INFO(cvedix_utils::string_format(
        "Total speed violations analyzed by MLLM: %d",
        mllm_trigger->get_violation_count()));

    file_src->detach_recursively();
}

#else

#include <iostream>
int main() {
    std::cerr << "This sample requires both TensorRT and LLM support." << std::endl;
    std::cerr << "Rebuild with: -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_LLM=ON" << std::endl;
    return 1;
}

#endif
