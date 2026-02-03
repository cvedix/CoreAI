/**
 * @file ba_area_enter_exit_sample.cpp
 * @brief Sample for area enter/exit BA node with OSD
 */

#include "cvedix/nodes/ba/cvedix_ba_area_enter_exit_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_ba_area_enter_exit_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
  CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
  CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_area"});
  CVEDIX_LOGGER_INIT();

  CVEDIX_INFO("===== BA Area Enter/Exit Sample =====");

  // Create source node (use an existing test video path; adjust as needed)
  auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
      "file_src", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.6);

  // Create detector
  auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "detector",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
      "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");

  // Create tracker
  auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
      "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

  // Define 2 rectangular areas for channel 0
  // Area 0: entrance-ish
  cvedix_objects::cvedix_rect area0(50, 150, 200, 200);
  // Area 1: restricted-ish
  cvedix_objects::cvedix_rect area1(350, 160, 200, 200);

  std::map<int, std::vector<cvedix_objects::cvedix_rect>> areas = {
      {0, {area0, area1}}};

  // Optional per-area configs (names/colors)
  cvedix_nodes::area_alert_config cfg0(true, true, "Entrance", cv::Scalar(0, 220, 0));
  cvedix_nodes::area_alert_config cfg1(true, true, "Restricted", cv::Scalar(0, 0, 220));
  std::map<int, std::vector<cvedix_nodes::area_alert_config>> configs = {
      {0, {cfg0, cfg1}}};

  // Create BA node with areas and configs
  auto ba_area = std::make_shared<cvedix_nodes::cvedix_ba_area_enter_exit_node>(
      "ba_area", areas, configs, false, false);

  // Create OSD and screen
  auto osd = std::make_shared<cvedix_nodes::cvedix_ba_area_enter_exit_osd_node>("osd");
//   auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

  // Optional use rtmp
  auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://127.0.0.1/live/9000");

  // Build pipeline
  detector->attach_to({file_src});
  tracker->attach_to({detector});
  ba_area->attach_to({tracker});
  osd->attach_to({ba_area});
  rtmp_des_0->attach_to({osd});
//   screen->attach_to({osd});

  CVEDIX_INFO("Pipeline built: src -> detector -> tracker -> ba_area -> osd -> screen");
  CVEDIX_INFO("Starting pipeline... Watch for area enter/exit alerts in top-left corner");

  file_src->start();

  // Wait for user to press Enter
  std::string wait;
  std::getline(std::cin, wait);

  file_src->detach_recursively();
  return 0;
}
