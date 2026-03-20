/**
 * @file plate_bytetrack_ocr_sample.cpp
 * @brief License Plate Detection + ByteTrack Tracking + PaddleOCR Recognition
 *
 * Pipeline:
 *   Video -> YOLOv11 TRT Plate Detector -> ByteTrack -> Best Plate Selector ->
 * PaddleOCR -> OSD -> Screen
 *
 * Features:
 * - High-speed license plate detection using YOLOv11 TensorRT
 * - Multi-plate tracking with persistent IDs using ByteTrack
 * - Best plate selection per track_id (largest, highest confidence, good aspect
 * ratio)
 * - PaddleOCR recognition on best crops
 * - OCR result caching per track_id (avoid redundant OCR)
 *
 * Requirements:
 * - cvedix compiled with -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_PADDLE=ON
 */

#if defined(CVEDIX_WITH_TRT) && defined(CVEDIX_WITH_PADDLE)

#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_plate_recogniton_ppocr3.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

std::atomic<bool> g_running{true};

void signal_handler(int signum) { g_running = false; }

/**
 * @brief Best plate crop info per track_id
 */
struct BestPlateInfo {
  int track_id;
  float best_score;     // Combined score: size * confidence * aspect_weight
  cv::Mat best_crop;    // Best crop image
  std::string ocr_text; // OCR result (cached)
  float ocr_confidence; // OCR confidence
  int frame_index;      // Frame where best crop was captured
  bool ocr_done;        // Whether OCR has been performed
  int last_seen_frame;  // Last frame this track was seen
};

/**
 * @brief Calculate quality score for plate selection
 * Higher score = better candidate for OCR
 */
float calculate_plate_score(int width, int height, float confidence) {
  // Size component (larger = better, normalized)
  float area = static_cast<float>(width * height);

  // Aspect ratio component (Vietnamese plates ~2.5-3.5 width/height)
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  float ideal_aspect = 3.0f; // Ideal aspect ratio for VN plates
  float aspect_diff = std::abs(aspect - ideal_aspect);
  float aspect_weight =
      1.0f / (1.0f + aspect_diff); // 1.0 for perfect, lower for deviation

  // Minimum size threshold
  if (width < 30 || height < 10) {
    return 0.0f; // Too small
  }

  // Combined score
  return area * confidence * aspect_weight;
}

void print_usage(const char *prog) {
  std::cout << "============================================================"
            << std::endl;
  std::cout << "  License Plate Detection + ByteTrack + PaddleOCR" << std::endl;
  std::cout << "============================================================"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << prog << " <engine> <video> <ocr_dir>" << std::endl;
  std::cout << std::endl;
  std::cout << "Arguments:" << std::endl;
  std::cout << "  engine   TRT engine for plate detection" << std::endl;
  std::cout << "  video    Input video file" << std::endl;
  std::cout << "  ocr_dir  PaddleOCR models directory" << std::endl;
  std::cout << std::endl;
  std::cout << "Example:" << std::endl;
  std::cout << "  " << prog << " \\" << std::endl;
  std::cout << "      ./models/plate-v1x-trt10.engine \\" << std::endl;
  std::cout << "      ./video/vietnam_plate.mp4 \\" << std::endl;
  std::cout << "      ./cvedix_data/models/text/ppocr" << std::endl;
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);

  CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
  CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_LOGGER_INIT();

  // Default parameters
  std::string engine_path =
      "./cvedix_data/models/tensorrt/license-plate-finetune-v1x-trt10.engine";
  std::string video_path = "./cvedix_data/test_video/vietnam_plate.mp4";
  std::string ocr_root = "./cvedix_data/models/text/ppocr";

  if (argc > 1 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    print_usage(argv[0]);
    return 0;
  }

  if (argc > 1)
    engine_path = argv[1];
  if (argc > 2)
    video_path = argv[2];
  if (argc > 3)
    ocr_root = argv[3];

  // PaddleOCR paths
  std::string det_dir = ocr_root + "/ch_PP-OCRv3_det_infer";
  std::string cls_dir = ocr_root + "/ch_ppocr_mobile_v2.0_cls_infer";
  std::string rec_dir = ocr_root + "/ch_PP-OCRv3_rec_infer";
  std::string keys_path = ocr_root + "/ppocr_keys_v1.txt";

  CVEDIX_INFO("============================================================");
  CVEDIX_INFO("  License Plate Detection + ByteTrack + PaddleOCR");
  CVEDIX_INFO("============================================================");
  CVEDIX_INFO("Engine: " + engine_path);
  CVEDIX_INFO("Video:  " + video_path);
  CVEDIX_INFO("OCR:    " + ocr_root);
  CVEDIX_INFO("============================================================");

  try {
    // ============================================
    // 1. SOURCE
    // ============================================
    auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "src", 0, video_path, 1.0f, true);

    // ============================================
    // 2. DETECTOR - YOLOv11 TensorRT
    // ============================================
    auto detector =
        std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
          "yolo_detector",
          "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
          "./cvedix_data/models/yolov11/onnx/labels.txt",
          0.45, 0.5, 0, cvedix_nodes::BackendType::ONNX);

    // ============================================
    // 3. TRACKER - ByteTrack
    // ============================================
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "bytetrack", cvedix_nodes::cvedix_track_for::NORMAL,
        0.3f, // track_thresh
        0.7f, // high_thresh
        0.8f, // match_thresh
        60,   // track_buffer
        30    // frame_rate
    );

    // ============================================
    // 4. RECOGNIZER - PaddleOCR (Secondary Infer)
    // ============================================
    auto recognizer =
        std::make_shared<cvedix_nodes::cvedix_plate_recogniton_ppocr3>(
            "ocr", det_dir, cls_dir, rec_dir, keys_path,
            std::vector<int>{0}, // Apply to class 0 (plate)
            30, 10,              // min width/height
            2,                   // padding
            false,               // use_tensorrt OFF (GPU native mode)
            "fp16"               // precision (GPU still uses fp16)
        );

    // ============================================
    // 5. OSD + DISPLAY
    // ============================================
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen =
        std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // ============================================
    // Build Pipeline
    // ============================================
    detector->attach_to({src});
    tracker->attach_to({detector});
    recognizer->attach_to({tracker});
    osd->attach_to({recognizer});
    screen->attach_to({osd});

    CVEDIX_INFO(
        "Pipeline: src -> detector -> bytetrack -> ocr -> osd -> screen");

    // ============================================
    // Statistics tracking with Best Plate Selection
    // ============================================
    std::map<int, BestPlateInfo> track_info_map;
    std::mutex stats_mutex;
    std::atomic<int> frame_count{0};

    auto stats_hook = [&](std::string node_name, int queue_size,
                          std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
      auto fm =
          std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
      if (!fm)
        return;

      int current_frame = frame_count++;

      std::lock_guard<std::mutex> lock(stats_mutex);

      for (const auto &target : fm->targets) {
        int tid = target->track_id;
        if (tid < 0)
          continue;

        // Calculate quality score
        float score = calculate_plate_score(target->width, target->height,
                                            target->primary_score);

        // Update best plate info
        auto it = track_info_map.find(tid);
        if (it == track_info_map.end()) {
          // New track
          BestPlateInfo info;
          info.track_id = tid;
          info.best_score = score;
          info.ocr_done = false;
          info.frame_index = current_frame;
          info.last_seen_frame = current_frame;

          // Get OCR result from secondary labels
          if (!target->secondary_labels.empty()) {
            info.ocr_text = target->secondary_labels[0];
            info.ocr_confidence = target->secondary_scores.empty()
                                      ? 0.0f
                                      : target->secondary_scores[0];
            info.ocr_done = true;

            CVEDIX_INFO("[Track " + std::to_string(tid) +
                        "] New plate: " + info.ocr_text +
                        " (score=" + std::to_string(score) + ")");
          }

          track_info_map[tid] = info;
        } else {
          // Update existing track
          it->second.last_seen_frame = current_frame;

          // Check if this is a better crop
          if (score >
              it->second.best_score * 1.1f) { // 10% improvement threshold
            it->second.best_score = score;
            it->second.frame_index = current_frame;

            // Update OCR if available
            if (!target->secondary_labels.empty()) {
              std::string new_text = target->secondary_labels[0];

              // Only update if different text or higher confidence
              if (new_text != it->second.ocr_text) {
                it->second.ocr_text = new_text;
                it->second.ocr_confidence = target->secondary_scores.empty()
                                                ? 0.0f
                                                : target->secondary_scores[0];

                CVEDIX_INFO("[Track " + std::to_string(tid) +
                            "] Better plate: " + new_text +
                            " (score=" + std::to_string(score) + ")");
              }
            }
          }
        }
      }

      // Print summary every 100 frames
      if (current_frame % 100 == 0 && current_frame > 0) {
        std::cout << "\r[Frame " << current_frame
                  << "] Active tracks: " << track_info_map.size() << std::flush;
      }
    };

    recognizer->set_meta_handled_hooker(stats_hook);

    // ============================================
    // Start
    // ============================================
    CVEDIX_INFO("Starting pipeline...");
    src->start();

    // Analysis Board
    cvedix_utils::cvedix_analysis_board board({src});
    board.display(1, false);

    CVEDIX_INFO("Press Enter to stop or Ctrl+C to exit.");

    std::thread input_thread([&]() {
      std::string wait;
      std::getline(std::cin, wait);
      g_running = false;
    });

    while (g_running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << std::endl;

    // ============================================
    // Final Summary
    // ============================================
    src->detach_recursively();

    CVEDIX_INFO("============================================================");
    CVEDIX_INFO("                    Recognition Results");
    CVEDIX_INFO("============================================================");

    {
      std::lock_guard<std::mutex> lock(stats_mutex);
      for (const auto &[tid, info] : track_info_map) {
        if (!info.ocr_text.empty()) {
          CVEDIX_INFO("Track " + std::to_string(tid) + ": " + info.ocr_text +
                      " (conf=" + std::to_string(info.ocr_confidence) +
                      ", score=" + std::to_string(info.best_score) + ")");
        }
      }
      CVEDIX_INFO("Total tracks: " + std::to_string(track_info_map.size()));
    }

    CVEDIX_INFO("============================================================");

    if (input_thread.joinable()) {
      input_thread.detach();
    }

  } catch (const std::exception &e) {
    CVEDIX_ERROR("Exception: " + std::string(e.what()));
    return -1;
  }

  return 0;
}

#else

#include <iostream>

int main() {
  std::cerr << "This sample requires CVEDIX_WITH_TRT and CVEDIX_WITH_PADDLE"
            << std::endl;
  std::cerr << "Please rebuild with:" << std::endl;
  std::cerr << "  cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_PADDLE=ON .."
            << std::endl;
  return 1;
}

#endif
