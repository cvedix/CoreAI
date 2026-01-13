/**
 * @file yolov11_face_bytetrack_sample.cpp
 * @brief YOLOv11 TensorRT Face Detection + ByteTrack Tracking
 *
 * Pipeline:
 *   Video -> YOLOv11 TRT Face Detector -> ByteTrack Tracker -> OSD -> Screen
 *
 * Features:
 * - High-speed face detection using YOLOv11 TensorRT
 * - Multi-face tracking with persistent IDs using ByteTrack
 * - 5-point facial landmarks
 * - Real-time visualization with track IDs
 *
 * Requirements:
 * - cvedix compiled with -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON
 * - TensorRT engine for YOLOv11 face detection
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <thread>

std::atomic<bool> g_running{true};

void signal_handler(int signum) { g_running = false; }

void print_usage(const char *prog) {
  std::cout << "============================================================"
            << std::endl;
  std::cout << "  YOLOv11 TensorRT Face Detection + ByteTrack Tracking"
            << std::endl;
  std::cout << "============================================================"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << prog << " [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Arguments:" << std::endl;
  std::cout << "  engine     TRT engine path (default: "
               "./cvedix_data/models/trt/face/yolov11_face_fp16.engine)"
            << std::endl;
  std::cout << "  video      Input video path (default: "
               "./cvedix_data/test_video/face.mp4)"
            << std::endl;
  std::cout << "  output     Output video directory (optional, omit to disable "
               "saving)"
            << std::endl;
  std::cout << "  conf       Detection confidence threshold (default: 0.5)"
            << std::endl;
  std::cout << "  nms        NMS threshold (default: 0.45)" << std::endl;
  std::cout << std::endl;
  std::cout << "ByteTrack Parameters (can be tuned in code):" << std::endl;
  std::cout << "  track_thresh:  Detection threshold for tracking (0.5)"
            << std::endl;
  std::cout << "  high_thresh:   High confidence threshold (0.9)" << std::endl;
  std::cout << "  match_thresh:  IOU matching threshold (0.8)" << std::endl;
  std::cout << "  track_buffer:  Frames to keep lost tracks (30)" << std::endl;
  std::cout << "  frame_rate:    Expected FPS (30)" << std::endl;
  std::cout << std::endl;
  std::cout << "Example:" << std::endl;
  std::cout << "  " << prog << " \\" << std::endl;
  std::cout << "      ./models/yolov11_face_fp16.engine \\" << std::endl;
  std::cout << "      ./video/input.mp4 \\" << std::endl;
  std::cout << "      ./output" << std::endl;
  std::cout << std::endl;
  std::cout << "Press Enter to stop, or Ctrl+C to exit." << std::endl;
}

int main(int argc, char **argv) {
  // Setup signal handler for graceful shutdown
  signal(SIGINT, signal_handler);

  CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
  CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_LOGGER_INIT();

  // Default parameters
  std::string engine_path =
      "./cvedix_data/models/trt/face/yolov11_face_fp16.engine";
  std::string video_path = "./cvedix_data/test_video/face.mp4";
  std::string output_dir = ""; // Empty = no file output
  float conf_threshold = 0.5f;
  float nms_threshold = 0.45f;

  // ByteTrack parameters
  float track_thresh = 0.5f; // Minimum confidence to be tracked
  float high_thresh = 0.9f;  // High confidence for first association
  float match_thresh = 0.8f; // IOU threshold for matching
  int track_buffer = 30;     // Frames to keep lost tracks
  int frame_rate = 30;       // Expected FPS

  // Parse command line
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
    output_dir = argv[3];
  if (argc > 4)
    conf_threshold = std::stof(argv[4]);
  if (argc > 5)
    nms_threshold = std::stof(argv[5]);

  CVEDIX_INFO("============================================================");
  CVEDIX_INFO("  YOLOv11 TensorRT Face Detection + ByteTrack Tracking");
  CVEDIX_INFO("============================================================");
  CVEDIX_INFO("Configuration:");
  CVEDIX_INFO("  Engine:    " + engine_path);
  CVEDIX_INFO("  Video:     " + video_path);
  CVEDIX_INFO("  Output:    " +
              (output_dir.empty() ? "(disabled)" : output_dir));
  CVEDIX_INFO("  Conf:      " + std::to_string(conf_threshold));
  CVEDIX_INFO("  NMS:       " + std::to_string(nms_threshold));
  CVEDIX_INFO("ByteTrack:");
  CVEDIX_INFO("  Track:     " + std::to_string(track_thresh));
  CVEDIX_INFO("  High:      " + std::to_string(high_thresh));
  CVEDIX_INFO("  Match:     " + std::to_string(match_thresh));
  CVEDIX_INFO("  Buffer:    " + std::to_string(track_buffer) + " frames");
  CVEDIX_INFO("============================================================");

  try {
    // ============================================
    // 1. SOURCE - Video file input
    // ============================================
    auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "src",      // node name
        0,          // channel index
        video_path, // video path
        1.0f,       // speed multiplier
        true        // loop
    );

    // ============================================
    // 2. DETECTOR - YOLOv11 TensorRT Face Detector
    // ============================================
    auto detector =
        std::make_shared<cvedix_nodes::cvedix_trt_yolov11_face_detector_node>(
            "yolov11_face", // node name
            engine_path,    // TRT engine
            conf_threshold, // confidence threshold
            nms_threshold   // NMS threshold
        );

    // ============================================
    // 3. TRACKER - ByteTrack for multi-face tracking
    // ============================================
    // ByteTrack assigns persistent IDs to detected faces across frames
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "bytetrack",
        cvedix_nodes::cvedix_track_for::FACE, // Track face targets
        track_thresh, // Detection confidence threshold for tracking
        high_thresh,  // High confidence threshold for first association
        match_thresh, // IOU threshold for matching
        track_buffer, // Number of frames to keep lost tracks
        frame_rate    // Expected frame rate
    );

    // ============================================
    // 4. OSD - Draw bounding boxes with track IDs
    // ============================================
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd");

    // ============================================
    // 5. DESTINATIONS - Screen display + optional file output
    // ============================================
    auto screen =
        std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    std::shared_ptr<cvedix_nodes::cvedix_file_des_node> file_out = nullptr;
    if (!output_dir.empty()) {
      file_out = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
          "file_out", 0,
          output_dir,                        // save directory
          "face_tracking_",                  // filename prefix
          30,                                // 30 minutes max per file
          cvedix_objects::cvedix_size{0, 0}, // auto resolution
          4096,                              // bitrate
          true                               // OSD enabled
      );
    }

    // ============================================
    // Build Pipeline
    // ============================================
    // Flow: src -> detector -> tracker -> osd -> screen/file
    detector->attach_to({src});
    tracker->attach_to({detector});
    osd->attach_to({tracker});
    screen->attach_to({osd});
    if (file_out) {
      file_out->attach_to({osd});
    }

    CVEDIX_INFO(
        std::string(
            "Pipeline: src -> yolov11_face -> bytetrack -> osd -> screen") +
        (file_out ? " + file" : ""));

    // ============================================
    // Statistics tracking
    // ============================================
    std::atomic<int> frame_count{0};
    std::atomic<int> total_detections{0};
    std::map<int, int> track_id_counts; // track_id -> appearance count
    std::mutex stats_mutex;

    auto stats_hook = [&](std::string node_name, int queue_size,
                          std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
      auto fm =
          std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
      if (fm) {
        frame_count++;
        int faces_in_frame = fm->face_targets.size();
        total_detections += faces_in_frame;

        // Track unique IDs
        {
          std::lock_guard<std::mutex> lock(stats_mutex);
          for (const auto &face : fm->face_targets) {
            track_id_counts[face->track_id]++;
          }
        }

        // Progress update every 100 frames
        if (frame_count % 100 == 0) {
          std::lock_guard<std::mutex> lock(stats_mutex);
          std::cout << "\r[Frame " << frame_count << "] "
                    << "Faces: " << faces_in_frame
                    << ", Unique tracks: " << track_id_counts.size()
                    << ", Total detections: " << total_detections << std::flush;
        }
      }
    };
    tracker->set_meta_handled_hooker(stats_hook);

    // ============================================
    // Start pipeline
    // ============================================
    CVEDIX_INFO("Starting pipeline...");
    CVEDIX_INFO("Press Enter to stop or Ctrl+C to exit.");

    src->start();

    // Optional: Display analysis board for debugging
    // cvedix_utils::cvedix_analysis_board board({src});
    // board.display(1, false);

    // Wait for user input or signal
    std::thread input_thread([&]() {
      std::string wait;
      std::getline(std::cin, wait);
      g_running = false;
    });

    while (g_running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << std::endl; // New line after progress

    // ============================================
    // Cleanup & Statistics
    // ============================================
    src->detach_recursively();

    // Wait for file to be written
    if (file_out) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    CVEDIX_INFO("============================================================");
    CVEDIX_INFO("                    Processing Summary");
    CVEDIX_INFO("============================================================");
    CVEDIX_INFO("Total frames processed: " +
                std::to_string(frame_count.load()));
    CVEDIX_INFO("Total face detections: " +
                std::to_string(total_detections.load()));
    {
      std::lock_guard<std::mutex> lock(stats_mutex);
      CVEDIX_INFO("Unique track IDs: " +
                  std::to_string(track_id_counts.size()));

      // Find longest tracked face
      int max_track_id = -1;
      int max_appearances = 0;
      for (const auto &[id, count] : track_id_counts) {
        if (count > max_appearances) {
          max_appearances = count;
          max_track_id = id;
        }
      }
      if (max_track_id >= 0) {
        CVEDIX_INFO("Longest tracked face: ID " + std::to_string(max_track_id) +
                    " (" + std::to_string(max_appearances) + " frames)");
      }
    }

    if (!output_dir.empty()) {
      CVEDIX_INFO("Output saved to: " + output_dir);
    }
    CVEDIX_INFO("============================================================");

    if (input_thread.joinable()) {
      input_thread.detach(); // Let it finish on its own
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
  std::cerr << "============================================================"
            << std::endl;
  std::cerr << "  This sample requires TensorRT support" << std::endl;
  std::cerr << "============================================================"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "Please rebuild cvedix with:" << std::endl;
  std::cerr << "  cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON .."
            << std::endl;
  std::cerr << "  make -j$(nproc)" << std::endl;
  return 1;
}

#endif
