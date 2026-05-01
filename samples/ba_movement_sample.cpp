/**
 * @file ba_movement_sample.cpp
 * @brief Sample for movement BA node with OSD
 *
 * Usage:
 *   ./ba_movement_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
 */

#include "cvedix/nodes/ba/cvedix_ba_movement_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/objects/shapes/cvedix_rect.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

int main(int argc, char** argv) {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_movement"});
  CVEDIX_LOGGER_INIT();

  auto out_cfg = sample_helper::parse_output_args(argc, argv);

  CVEDIX_INFO("===== BA Movement Sample =====");

  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, "./cvedix_data/video/vehicle_count.mp4", 0.6);

  auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
    "detector",
    "./cvedix_data/models/yolov26/openvino/yolo26n.xml",
    cvedix_nodes::YoloVersion::YOLO26, "", 0.25f, 0.45f, 0,
    cvedix_nodes::BackendType::OPENVINO);

  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  std::vector<cvedix_objects::cvedix_point> area0 = {{50, 150}, {250, 100}, {250, 350}, {50, 350}};
  std::vector<cvedix_objects::cvedix_point> area1 = {{350, 160}, {520, 100}, {550, 360}, {350, 360}};

  std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas = {{0, {area0, area1}}};

  cvedix_nodes::movement_alert_config cfg0(true, "Config1", cv::Scalar(0, 220, 0), cvedix_objects::cvedix_rect_anchor_point::CENTER, {"car"});
  cvedix_nodes::movement_alert_config cfg1(true, "Config2", cv::Scalar(0, 0, 220), cvedix_objects::cvedix_rect_anchor_point::CENTER, {"person", "truck"});
  std::map<int, std::vector<cvedix_nodes::movement_alert_config>> configs = {{0, {cfg0, cfg1}}};

  auto ba_movement = std::make_shared<cvedix_nodes::cvedix_ba_movement_node>(
      "ba_movement", areas, configs, false, false);

  auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
  auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src});

  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_movement->attach_to({tracker});
  osd->attach_to({ba_movement});
  output.des_node->attach_to({osd});

  file_src->start();
    sample_helper::init_board(output);
  sample_helper::print_output_info(out_cfg);

  std::string wait;
  std::getline(std::cin, wait);
  file_src->detach_recursively();
  return 0;
}
