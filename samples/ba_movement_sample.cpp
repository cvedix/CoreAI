/**
 * @file ba_movement_sample.cpp
 * @brief Sample for movement BA node with OSD
 */

#include "cvedix/nodes/ba/cvedix_ba_movement_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_ov_yolov11_det_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/objects/shapes/cvedix_rect.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_movement"});
  CVEDIX_LOGGER_INIT();

  CVEDIX_INFO("===== BA Movement Sample =====");

  // Create source node (use an existing test video path; adjust as needed)
  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.6);


  // Create detector
  auto detector = std::make_shared<cvedix_nodes::cvedix_ov_yolov11_det_node>(
    "detector",
    "./cvedix_data/models/ov/yolov11/yolo11n_openvino_model/yolo11n.xml",
    "CPU",
    0.25f,
    0.45f
  );
//   // Create detector
//   auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
//       "detector",
//       "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
//       "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
//       "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");

  // Create tracker
  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  // Define 2 polygonal areas for channel 0
  // Area 0: entrance zone (rectangular polygon)
  std::vector<cvedix_objects::cvedix_point> area0 = {
      {50, 150}, {250, 100}, {250, 350}, {50, 350}
  };
  // Area 1: restricted zone (rectangular polygon)
  std::vector<cvedix_objects::cvedix_point> area1 = {
      {350, 160}, {520, 100}, {550, 360}, {350, 360}
  };

  std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas = {
      {0, {area0, area1}}};

  // Optional per-area configs (names/colors)
  cvedix_nodes::movement_alert_config cfg0(true, "Config1", cv::Scalar(0, 220, 0), cvedix_objects::cvedix_rect_anchor_point::CENTER, {"car"});
  cvedix_nodes::movement_alert_config cfg1(true, "Config2", cv::Scalar(0, 0, 220), cvedix_objects::cvedix_rect_anchor_point::CENTER, {"person", "truck"});
  std::map<int, std::vector<cvedix_nodes::movement_alert_config>> configs = {
      {0, {cfg0, cfg1}}};

  // Create BA node with areas and configs
  auto ba_movement = std::make_shared<cvedix_nodes::cvedix_ba_movement_node>(
      "ba_movement", areas, configs, false, false);
      
  // Create OSD and screen
  auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
//   auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

  // Optional use rtmp
  auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://127.0.0.1/live/9000");

  // Build pipeline
  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_movement->attach_to({tracker});
  osd->attach_to({ba_movement});
  rtmp_des_0->attach_to({osd});
//   screen->attach_to({osd});

  CVEDIX_INFO("Pipeline built: src -> detector -> tracker -> ba_movement -> osd -> rtmp_des_0");
  CVEDIX_INFO("Starting pipeline... Watch for movement alerts in top-left corner");
  file_src->start();

  // Wait for user to press Enter
  std::string wait;
  std::getline(std::cin, wait);

  file_src->detach_recursively();
  return 0;
}
