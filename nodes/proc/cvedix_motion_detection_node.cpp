#include "cvedix_motion_detection_node.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace cvedix_nodes {

cvedix_motion_detection_node::cvedix_motion_detection_node(
    std::string node_name,
    const motion_detection_config &config)
    : cvedix_node(std::move(node_name)), _config(config) {
    reset_subtractor_locked();
    initialized();
}

cvedix_motion_detection_node::~cvedix_motion_detection_node() {
    deinitialized();
}

void cvedix_motion_detection_node::normalize_config_locked() {
    _config.history = std::max(1, _config.history);
    _config.var_threshold = std::max(0.1, _config.var_threshold);
    _config.min_area_ratio =
        std::max(0.0, std::min(1.0, _config.min_area_ratio));
    _config.warmup_frames = std::max(0, _config.warmup_frames);
    _config.hold_frames = std::max(0, _config.hold_frames);
    _config.processing_width = std::max(64, _config.processing_width);
}

void cvedix_motion_detection_node::reset_subtractor_locked() {
    normalize_config_locked();
    _subtractor = cv::createBackgroundSubtractorMOG2(
        _config.history, _config.var_threshold, false);
    _warmup_remaining = _config.warmup_frames;
    _hold_remaining = 0;
}

void cvedix_motion_detection_node::update_config(
    const motion_detection_config &config) {
    std::lock_guard<std::mutex> lock(_config_mtx);
    const bool reset_model =
        config.history != _config.history ||
        config.var_threshold != _config.var_threshold ||
        config.processing_width != _config.processing_width;
    _config = config;
    if (reset_model || !_subtractor) {
        reset_subtractor_locked();
    } else {
        normalize_config_locked();
    }
}

motion_detection_config cvedix_motion_detection_node::get_config() const {
    std::lock_guard<std::mutex> lock(_config_mtx);
    return _config;
}

std::string cvedix_motion_detection_node::to_string() {
    return "cvedix_motion_detection_node[" + node_name + "]";
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_motion_detection_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    ++_frames_seen;
    if (!meta || meta->frame.empty()) {
        return meta;
    }

    std::lock_guard<std::mutex> lock(_config_mtx);
    if (!_config.enabled) {
        _motion_active = false;
        _last_region_count = 0;
        return meta;
    }

    cv::Mat processing_frame;
    const double scale = meta->frame.cols > _config.processing_width
        ? static_cast<double>(_config.processing_width) / meta->frame.cols
        : 1.0;
    if (scale < 1.0) {
        cv::resize(meta->frame, processing_frame, cv::Size(), scale, scale,
                   cv::INTER_AREA);
    } else {
        processing_frame = meta->frame;
    }

    cv::Mat gray;
    cv::cvtColor(processing_frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    cv::Mat foreground;
    _subtractor->apply(gray, foreground);
    cv::threshold(foreground, foreground, 200, 255, cv::THRESH_BINARY);
    const auto kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(foreground, foreground, cv::MORPH_OPEN, kernel);
    cv::dilate(foreground, foreground, kernel, cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(foreground, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    const double min_area = foreground.total() * _config.min_area_ratio;
    cv::Mat filtered = cv::Mat::zeros(foreground.size(), CV_8UC1);
    int region_count = 0;
    for (const auto &contour : contours) {
        if (cv::contourArea(contour) < min_area) {
            continue;
        }
        cv::drawContours(filtered,
                         std::vector<std::vector<cv::Point>>(1, contour),
                         0, cv::Scalar(255), cv::FILLED);
        ++region_count;
    }

    bool active = region_count > 0;
    if (_warmup_remaining > 0) {
        --_warmup_remaining;
        active = true;
    }
    if (active) {
        _hold_remaining = _config.hold_frames;
    } else if (_hold_remaining > 0) {
        --_hold_remaining;
        active = true;
    }

    if (filtered.size() != meta->frame.size()) {
        cv::resize(filtered, meta->mask, meta->frame.size(), 0, 0,
                   cv::INTER_NEAREST);
    } else {
        meta->mask = filtered;
    }

    _last_region_count = region_count;
    _motion_active = active;
    if (region_count > 0) {
        ++_motion_frames;
    }

    if (_config.drop_static_frames && !active) {
        return nullptr;
    }
    return meta;
}

} // namespace cvedix_nodes
