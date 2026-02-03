/**
 * @file cvedix_ba_area_enter_exit_osd_node.h
 * @brief OSD for area enter/exit behavior analysis
 *
 * Highlights areas when enter/exit events occur and shows alerts in the
 * corner: "ID 1 - Enter" or "ID 2 - Exit".
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <opencv2/freetype.hpp>
#include <map>
#include <string>
#include <vector>

namespace cvedix_nodes {

struct osd_active_alert {
  std::string text;
  cv::Scalar color;
  int ttl_frames;
};

struct osd_active_poly {
  std::vector<cvedix_objects::cvedix_point> poly;
  cv::Scalar color;
  int ttl_frames;
};

/**
 * @brief OSD node for area enter/exit BA events
 */
class cvedix_ba_area_enter_exit_osd_node : public cvedix_node {
private:
  cv::Ptr<cv::freetype::FreeType2> ft2;

  // Recent textual alerts per channel (displayed in corner)
  std::map<int, std::vector<osd_active_alert>> recent_alerts;

  // Active polygons to highlight per channel
  std::map<int, std::vector<osd_active_poly>> active_polys;

  // How long (in frames) to show alerts / highlights
  int alert_ttl_frames = 50;

protected:
  virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
      std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
  cvedix_ba_area_enter_exit_osd_node(std::string node_name,
                                     std::string font = "");
  ~cvedix_ba_area_enter_exit_osd_node();
};

} // namespace cvedix_nodes
