/**
 * @file ba_multiline_crossline_test.cpp
 * @brief Test multi-line per channel feature for cvedix_ba_line_crossline_node
 */

#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_ba_line_crossline_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
 * ## Multi-Line BA Crossline Test Sample ##
 * Tests the new multi-line per channel feature.
 * Creates 2 lines for channel 0 and verifies:
 * 1. Both lines detect crossings independently
 * 2. Events include correct line_index in ba_label
 * 3. Counters work per-line
 */

int main() {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_crossline"});
  CVEDIX_LOGGER_INIT();

  CVEDIX_INFO("===== Multi-Line BA Crossline Test =====");

  // Create source node
  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, "./cvedix_data/test_video/vehicle_count.mp4",
      0.4 // resize
  );

  // Create detector
  auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "detector",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
      "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");

  // Create tracker
  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  // ========== NEW: Multiple lines per channel ==========
  // Define 2 lines for channel 0
  cvedix_objects::cvedix_line line0(
      cvedix_objects::cvedix_point(0, 200),  // Start
      cvedix_objects::cvedix_point(700, 180) // End
  );
  cvedix_objects::cvedix_line line1(
      cvedix_objects::cvedix_point(0, 280),  // Start
      cvedix_objects::cvedix_point(700, 260) // End
  );

  // Multi-line map: channel 0 -> vector of 2 lines
  std::map<int, std::vector<cvedix_objects::cvedix_line>> lines = {
      {0, {line0, line1}}};

  auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
      "ba_crossline", lines,
      false, // no image recording
      false  // no video recording
  );

  CVEDIX_INFO("Created multi-line crossline node:");
  CVEDIX_INFO("  Channel 0: Line 0 at y=200, Line 1 at y=280");
  CVEDIX_INFO("  Total lines for channel 0: " +
              std::to_string(ba_crossline->get_line_count(0)));

  // Test add_line API
  int new_line_idx = ba_crossline->add_line(
      0, cvedix_objects::cvedix_line(cvedix_objects::cvedix_point(100, 350),
                                     cvedix_objects::cvedix_point(600, 350)));
  CVEDIX_INFO("Added new line dynamically, index: " +
              std::to_string(new_line_idx));
  CVEDIX_INFO("  Total lines for channel 0 after add: " +
              std::to_string(ba_crossline->get_line_count(0)));

  // Test remove_line API
  ba_crossline->remove_line(0, new_line_idx);
  CVEDIX_INFO("Removed line " + std::to_string(new_line_idx) +
              " from channel 0");
  CVEDIX_INFO("  Total lines for channel 0 after remove: " +
              std::to_string(ba_crossline->get_line_count(0)));

  // Create OSD and screen
  auto osd =
      std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_osd_node>("osd");
  auto screen =
      std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

  // Build pipeline
  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_crossline->attach_to({tracker});
  osd->attach_to({ba_crossline});
  screen->attach_to({osd});

  CVEDIX_INFO("Pipeline built: src -> detector -> tracker -> ba_crossline -> "
              "osd -> screen");
  CVEDIX_INFO("Starting pipeline... Watch for 'cross line 0' and 'cross line "
              "1' events");
  CVEDIX_INFO("===========================================");

  file_src->start();

  // Wait for user to press Enter
  std::string wait;
  std::getline(std::cin, wait);

  // Print final counters
  CVEDIX_INFO("===== Final Counters =====");
  CVEDIX_INFO("  Line 0 crossings: " +
              std::to_string(ba_crossline->get_crossline_count(0, 0)));
  CVEDIX_INFO("  Line 1 crossings: " +
              std::to_string(ba_crossline->get_crossline_count(0, 1)));

  file_src->detach_recursively();
  return 0;
}
