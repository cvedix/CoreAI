/**
 * @file ba_multiline_crossline_test.cpp
 * @brief Test multi-line per channel feature for cvedix_ba_line_crossline_node
 *
 * Usage:
 *   ./ba_multiline_crossline_test [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 */

#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

int main(int argc, char** argv) {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_crossline"});
  CVEDIX_LOGGER_INIT();

  auto out_cfg = sample_helper::parse_output_args(argc, argv);

  CVEDIX_INFO("===== Multi-Line BA Crossline Test =====");

  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, "./cvedix_data/video/vehicle_count.mp4", 0.4);

  auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "yolo_detector",
      "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
      cvedix_nodes::YoloVersion::YOLO11,
      "./cvedix_data/models/yolov11/onnx/labels.txt",
      0.45, 0.5, 0, cvedix_nodes::BackendType::ONNX);

  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  // Multiple lines per channel
  cvedix_objects::cvedix_line line0(
      cvedix_objects::cvedix_point(0, 200), cvedix_objects::cvedix_point(700, 180));
  cvedix_objects::cvedix_line line1(
      cvedix_objects::cvedix_point(0, 280), cvedix_objects::cvedix_point(700, 260));

  std::map<int, std::vector<cvedix_objects::cvedix_line>> lines = {{0, {line0, line1}}};

  auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
      "ba_crossline", lines, false, false);

  CVEDIX_INFO("Created multi-line crossline node:");
  CVEDIX_INFO("  Channel 0: Line 0 at y=200, Line 1 at y=280");
  CVEDIX_INFO("  Total lines for channel 0: " +
              std::to_string(ba_crossline->get_line_count(0)));

  // Test add_line API
  int new_line_idx = ba_crossline->add_line(
      0, cvedix_objects::cvedix_line(cvedix_objects::cvedix_point(100, 350),
                                     cvedix_objects::cvedix_point(600, 350)));
  CVEDIX_INFO("Added new line dynamically, index: " + std::to_string(new_line_idx));
  CVEDIX_INFO("  Total lines for channel 0 after add: " +
              std::to_string(ba_crossline->get_line_count(0)));

  // Test remove_line API
  ba_crossline->remove_line(0, new_line_idx);
  CVEDIX_INFO("Removed line " + std::to_string(new_line_idx) + " from channel 0");
  CVEDIX_INFO("  Total lines for channel 0 after remove: " +
              std::to_string(ba_crossline->get_line_count(0)));

  auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

  // create output destination based on --mode
  auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src});

  // Build pipeline
  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_crossline->attach_to({tracker});
  osd->attach_to({ba_crossline});
  output.des_node->attach_to({osd});

  CVEDIX_INFO("Pipeline built: src -> detector -> tracker -> ba_crossline -> osd -> " +
              sample_helper::mode_string(out_cfg.mode));

  file_src->start();
    sample_helper::init_board(output);
  sample_helper::print_output_info(out_cfg);

  std::string wait;
  std::getline(std::cin, wait);

  // Print final counters
  CVEDIX_INFO("===== Final Counters =====");
  CVEDIX_INFO("  Line 0 crossings: " + std::to_string(ba_crossline->get_crossline_count(0, 0)));
  CVEDIX_INFO("  Line 1 crossings: " + std::to_string(ba_crossline->get_crossline_count(0, 1)));

  file_src->detach_recursively();
  return 0;
}
