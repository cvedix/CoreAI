/**
 * @file wrong_way_detection_sample.cpp
 * @brief Detect vehicles going wrong direction (DOWN) in video
 *
 * Pipeline: Video -> Detector -> Tracker -> Crossline (OUT only) -> OSD -> Screen
 * 
 * Direction:
 *   IN = moving from below line to above (UP direction)
 *   OUT = moving from above line to below (DOWN direction - wrong way)
 */

#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_ba_line_crossline_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main(int argc, char **argv) {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_crossline"});
  CVEDIX_LOGGER_INIT();

  CVEDIX_INFO("===== Wrong-Way Detection (DOWN direction) Sample =====");

  // Default video or use command line argument
  std::string video_path = "./cvedix_data/test_video/vietnam_plate.mp4";
  if (argc > 1) {
    video_path = argv[1];
  }

  CVEDIX_INFO("Video: " + video_path);

  // 1. Source node
  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, video_path, 0.5 // resize to 0.5x for faster processing
  );

  // 2. Detector - YOLO for vehicle detection
  auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "detector",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
      "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");

  // 3. Tracker - ByteTrack for persistent tracking
  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  // 4. Crossline for detecting WRONG-WAY (DOWN/OUT direction only)
  //    Configure line across the road
  //    Direction: OUT = moving from above to below (DOWN = wrong way)
  cvedix_objects::cvedix_line detection_line(
      cvedix_objects::cvedix_point(0, 200),   // Start point (left)
      cvedix_objects::cvedix_point(700, 180)  // End point (right)
  );

  // Create crossline config with:
  // - Red color for wrong-way warning
  // - "WRONG_WAY" label
  // - OUT direction (down = wrong way)
  cvedix_nodes::crossline_config wrong_way_config(
      detection_line,
      cv::Scalar(0, 0, 255),  // Red color (BGR)
      "WRONG_WAY",            // Label
      cvedix_objects::cvedix_ba_direct_type::OUT  // Detect only DOWN direction
  );

  auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
      "ba_crossline",
      std::map<int, std::vector<cvedix_objects::cvedix_line>>{},  // Empty init
      false,  // no image recording
      false   // no video recording
  );

  // Add the wrong-way detection line with config
  ba_crossline->add_line(0, wrong_way_config);

  CVEDIX_INFO("Crossline configured:");
  CVEDIX_INFO("  Direction: OUT (DOWN = wrong way)");
  CVEDIX_INFO("  Color: RED (warning)");
  CVEDIX_INFO("  Label: WRONG_WAY");

  // 5. OSD for drawing crosslines
  auto osd = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_osd_node>("osd");

  // 6. Screen output
  auto screen =
      std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

  // Optional: File output
  std::shared_ptr<cvedix_nodes::cvedix_file_des_node> file_out = nullptr;
  if (argc > 2) {
    std::string output_dir = argv[2];
    file_out = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
        "file_out", 0, output_dir, "wrong_way_",
        30,  // max 30 min per file
        cvedix_objects::cvedix_size{0, 0}, 4096, true);
    CVEDIX_INFO("Output: " + output_dir);
  }

  // Build pipeline
  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_crossline->attach_to({tracker});
  osd->attach_to({ba_crossline});
  screen->attach_to({osd});
  if (file_out) {
    file_out->attach_to({osd});
  }

  CVEDIX_INFO("Pipeline: src -> detector -> tracker -> ba_crossline -> osd -> screen");
  CVEDIX_INFO("Detecting vehicles going WRONG WAY (DOWN direction)...");
  CVEDIX_INFO("Press Enter to stop.");
  CVEDIX_INFO("=======================================================");

  file_src->start();

  // Wait for user
  std::string wait;
  std::getline(std::cin, wait);

  // Print final stats
  CVEDIX_INFO("===== Wrong-Way Detection Results =====");
  CVEDIX_INFO("  Total wrong-way crossings: " +
              std::to_string(ba_crossline->get_crossline_count(0, 0)));

  file_src->detach_recursively();
  return 0;
}
