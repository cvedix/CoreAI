#ifdef CVEDIX_WITH_PADDLE
#include "cvedix_plate_recogniton_ppocr3.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <numeric>

namespace cvedix_nodes {

cvedix_plate_recogniton_ppocr3::cvedix_plate_recogniton_ppocr3(
    std::string node_name, std::string det_model_dir, std::string cls_model_dir,
    std::string rec_model_dir, std::string rec_char_dict_path,
    std::vector<int> p_class_ids_applied_to, int min_width_applied_to,
    int min_height_applied_to, int crop_padding, bool use_tensorrt,
    std::string precision)
    : // Initialize base with empty model path as we use PaddleOCR directly
      cvedix_secondary_infer_node(node_name,
                                  "",  // model_path
                                  "",  // config
                                  "",  // labels
                                  640, // dummy width
                                  640, // dummy height
                                  1,   // batch size (will be handled by paddle)
                                  p_class_ids_applied_to, min_width_applied_to,
                                  min_height_applied_to, crop_padding, 1.0,
                                  cv::Scalar(0), cv::Scalar(0), false, false),
      det_model_dir(det_model_dir), cls_model_dir(cls_model_dir),
      rec_model_dir(rec_model_dir), rec_char_dict_path(rec_char_dict_path) {
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Initializing PaddleOCR (TensorRT: %s, Precision: %s)...",
      node_name.c_str(), use_tensorrt ? "ON" : "OFF", precision.c_str()));
  // Initialize PaddleOCR
  ocr = std::make_shared<PaddleOCR::PPOCR>(det_model_dir, cls_model_dir,
                                           rec_model_dir, rec_char_dict_path,
                                           use_tensorrt, precision);

  this->initialized();
  CVEDIX_INFO(cvedix_utils::string_format("[%s] Initialized successfully",
                                          node_name.c_str()));
}

cvedix_plate_recogniton_ppocr3::~cvedix_plate_recogniton_ppocr3() {
  deinitialized();
}

void cvedix_plate_recogniton_ppocr3::postprocess(
    const std::vector<cv::Mat> &raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>
        &frame_meta_with_batch) {
  // Not used as we override run_infer_combinations
}

void cvedix_plate_recogniton_ppocr3::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>
        &frame_meta_with_batch) {
  // Ensure single batch for now as per design pattern often seen here, or
  // handle batch secondary_infer_node::prepare usually helps crop the images.

  std::vector<cv::Mat> mats_to_infer;

  // Use the base class prepare method to get crops
  // BUT cvedix_secondary_infer_node::prepare signature might differ or not
  // return the mapping to targets easily? Let's check
  // cvedix_secondary_infer_node.h if possible, but assuming it populates
  // mats_to_infer.

  // Actually, secondary_infer_node::prepare usually collects crops from all
  // frames in batch. We need to know which crop belongs to which target.
  // cvedix_secondary_infer_node typically maintains an internal mapping or we
  // need to replicate the cropping logic if we want to be sure. Looking at
  // cvedix_classifier_node, it calls `cvedix_secondary_infer_node::prepare`
  // (implied/inherited) but overrides `postprocess` `postprocess` receives
  // raw_outputs. But here we want to run PaddleOCR on the crops directly.

  // Re-implementing cropping logic to be safe and have direct access to targets

  auto start_time = std::chrono::system_clock::now();
  int total_crops = 0;

  // Map: index in mats_to_infer -> pair<frame_index, target_index>
  std::vector<std::pair<int, int>> map_crop_to_target;

  for (int i = 0; i < frame_meta_with_batch.size(); ++i) {
    auto &frame_meta = frame_meta_with_batch[i];

    for (int j = 0; j < frame_meta->targets.size(); ++j) {
      auto &target = frame_meta->targets[j];

      // Check if we should apply to this target
      if (!need_apply(target->primary_class_id, target->width,
                      target->height)) {
        continue;
      }

      // Crop
      cv::Rect crop_rect =
          cv::Rect(target->x, target->y, target->width, target->height);

      // Apply padding
      if (crop_padding > 0) {
        crop_rect.x = std::max(0, crop_rect.x - crop_padding);
        crop_rect.y = std::max(0, crop_rect.y - crop_padding);
        crop_rect.width = std::min(frame_meta->frame.cols - crop_rect.x,
                                   crop_rect.width + 2 * crop_padding);
        crop_rect.height = std::min(frame_meta->frame.rows - crop_rect.y,
                                    crop_rect.height + 2 * crop_padding);
      }

      if (crop_rect.width <= 0 || crop_rect.height <= 0)
        continue;

      cv::Mat crop = frame_meta->frame(crop_rect).clone();
      mats_to_infer.push_back(crop);

      map_crop_to_target.push_back({i, j});
      total_crops++;
    }
  }

  if (mats_to_infer.empty()) {
    return;
  }

  auto prepare_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start_time);
  start_time = std::chrono::system_clock::now();

  // Call PaddleOCR
  auto ocr_results = ocr->ocr(mats_to_infer);

  auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start_time);

  // Process results
  for (size_t k = 0; k < ocr_results.size(); ++k) {
    auto &res = ocr_results[k];

    // Get corresponding target
    int frame_idx = map_crop_to_target[k].first;
    int target_idx = map_crop_to_target[k].second;
    auto &target = frame_meta_with_batch[frame_idx]->targets[target_idx];
    // ...

    if (res.empty())
      continue;

    std::string full_text = "";
    float total_score = 0;

    // Concatenate text lines (for multi-line plates)
    // Or maybe just take the one with highest score, or sort by vertical
    // position? Usually plates are 1 or 2 lines. Let's concatenate with space
    // or hyphen, or just raw concatenation? User requirement: "plate
    // recognition". Typical storage is flat string.

    // We sort by Y coordinate first (top to bottom), then X?
    // PaddleOCR usually returns in some order, but let's trust it for now.
    // Often license plates are 2 lines:
    // 51A
    // 123.45
    // Reading order top-left to bottom-right is usually good.

    for (auto &item : res) {
      if (full_text.length() > 0)
        full_text += "-"; // Separator
      full_text += item.text;
      total_score += item.score;
    }
    float avg_score = res.empty() ? 0 : total_score / res.size();

    // Store result
    target->secondary_class_ids.push_back(0); // Dummy class ID for text
    target->secondary_labels.push_back(full_text);
    target->secondary_scores.push_back(avg_score);

    // Also update description for display
    target->primary_label = full_text;

    // Log recognized plate text
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Plate recognized: %s (score=%.2f)", node_name.c_str(),
        full_text.c_str(), avg_score));
  }

  cvedix_infer_node::infer_combinations_time_cost(
      mats_to_infer.size(), prepare_time.count(), 0, infer_time.count(), 0);
}
} // namespace cvedix_nodes
#endif
