#include "cvedix_ba_area_enter_exit_osd_node.h"

namespace cvedix_nodes {

cvedix_ba_area_enter_exit_osd_node::cvedix_ba_area_enter_exit_osd_node(
    std::string node_name, std::string font)
    : cvedix_node(node_name) {
  if (!font.empty()) {
    ft2 = cv::freetype::createFreeType2();
    ft2->loadFontData(font, 0);
  }
  this->initialized();
}

cvedix_ba_area_enter_exit_osd_node::~cvedix_ba_area_enter_exit_osd_node() {
  deinitialized();
}

static cv::Scalar enter_color() { return cv::Scalar(0, 220, 0); }
static cv::Scalar exit_color() { return cv::Scalar(0, 0, 220); }

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_enter_exit_osd_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
  // Ensure osd frame exists
  if (meta->osd_frame.empty()) {
    meta->osd_frame = meta->frame.clone();
  }

  auto &canvas = meta->osd_frame;
  int channel = meta->channel_index;

  // Draw all targets (bounding box, tracks and id/labels) similar to other OSD nodes
  for (auto &i : meta->targets) {
    // track_id
    auto id = std::to_string(i->track_id);
    auto labels_to_display = i->primary_label;

    // tracked
    if (i->track_id != -1) {
      labels_to_display = "#" + id + " " + labels_to_display;
    }

    for (auto &label : i->secondary_labels) {
      labels_to_display += "|" + label;
    }

    // draw tracks if size>=2
    if (i->tracks.size() >= 2) {
      for (size_t n = 0; n < (i->tracks.size() - 1); n++) {
        auto p1 = i->tracks[n].track_point();
        auto p2 = i->tracks[n + 1].track_point();
        cv::line(canvas, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y),
                 cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
      }
    }

    cv::rectangle(canvas, cv::Rect(i->x, i->y, i->width, i->height),
                  cv::Scalar(255, 255, 0), 2);
    if (ft2 != nullptr) {
      ft2->putText(canvas, labels_to_display, cv::Point(i->x, i->y), 20,
                   cv::Scalar(255, 0, 255), cv::FILLED, cv::LINE_AA, true);
    } else {
      int baseline = 0;
      auto size = cv::getTextSize(labels_to_display, 1, 1, 1, &baseline);
      cvedix_utils::put_text_at_center_of_rect(
          canvas, labels_to_display,
          cv::Rect(i->x, i->y - size.height, size.width, size.height), true,
          1, 1, cv::Scalar(), cv::Scalar(179, 52, 255),
          cv::Scalar(179, 52, 255));
    }
  }

  // Decrement TTLs and remove expired active polygons
  if (active_polys.count(channel)) {
    auto &vec = active_polys[channel];
    for (auto it = vec.begin(); it != vec.end();) {
      it->ttl_frames--;
      if (it->ttl_frames <= 0) {
        it = vec.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Decrement TTLs and remove expired alerts
  if (recent_alerts.count(channel)) {
    auto &vec = recent_alerts[channel];
    for (auto it = vec.begin(); it != vec.end();) {
      it->ttl_frames--;
      if (it->ttl_frames <= 0) {
        it = vec.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Process BA results and create alerts/highlights
  for (auto &ba_result : meta->ba_results) {
    if (ba_result->type == cvedix_objects::cvedix_ba_type::AREA_ENTER ||
        ba_result->type == cvedix_objects::cvedix_ba_type::AREA_EXIT) {
      // Prepare polygon from involve_region_in_frame (expect 4 points)
      osd_active_poly ap;
      ap.color = (ba_result->type == cvedix_objects::cvedix_ba_type::AREA_ENTER)
                     ? enter_color()
                     : exit_color();
      ap.ttl_frames = alert_ttl_frames;

      for (auto &p : ba_result->involve_region_in_frame) {
        ap.poly.push_back(p);
      }

      active_polys[channel].push_back(ap);

      // Create textual alerts for involved targets
      for (auto tid : ba_result->involve_target_ids_in_frame) {
        osd_active_alert al;
        al.color = ap.color;
        al.ttl_frames = alert_ttl_frames;
        std::string type_label = (ba_result->type == cvedix_objects::cvedix_ba_type::AREA_ENTER) ? "Enter" : "Exit";
        al.text = cvedix_utils::string_format("ID %d - %s", tid, type_label.c_str());
        recent_alerts[channel].push_back(al);
      }
    }
  }

  // Draw active polygons
  if (active_polys.count(channel)) {
    for (auto &ap : active_polys[channel]) {
      std::vector<cv::Point> pts;
      for (auto &p : ap.poly) pts.push_back(cv::Point(p.x, p.y));
      if (pts.size() >= 2) {
        cv::polylines(canvas, pts, true, ap.color, 3, cv::LINE_AA);
        // also draw a semi-bold transparent fill approximation by drawing
        // multiple polylines inside (simple visual emphasis)
        for (int w = 6; w >= 2; w -= 2) {
          cv::polylines(canvas, pts, true, ap.color, w, cv::LINE_AA);
        }
      }
    }
  }

  // Draw recent alerts in corner
  int y = 20;
  int x = 20;
  if (recent_alerts.count(channel)) {
    for (auto &al : recent_alerts[channel]) {
      if (ft2 != nullptr) {
        ft2->putText(canvas, al.text, cv::Point(x, y), 20, al.color,
                     cv::FILLED, cv::LINE_AA, true);
      } else {
        cv::putText(canvas, al.text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX,
                    0.6, al.color, 2);
      }
      y += 24;
    }
  }

  return meta;
}

} // namespace cvedix_nodes
