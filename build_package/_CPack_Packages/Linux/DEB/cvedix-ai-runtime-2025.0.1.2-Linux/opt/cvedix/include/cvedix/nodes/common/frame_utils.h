#pragma once

#include <opencv2/imgproc.hpp>

#include "cvedix/objects/cvedix_frame_meta.h"

namespace cvedix_nodes {
    namespace utils {
        inline const cv::Mat& select_source_frame(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, bool use_osd) {
            return (use_osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame;
        }

        inline cv::Mat prepare_output_frame(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
                                            bool use_osd,
                                            const cvedix_objects::cvedix_size& target_size) {
            const auto& source = select_source_frame(meta, use_osd);
            if (target_size.width > 0 && target_size.height > 0 &&
                (source.cols != target_size.width || source.rows != target_size.height)) {
                cv::Mat resized;
                cv::resize(source, resized, cv::Size(target_size.width, target_size.height));
                return resized;
            }
            return source;
        }
    }
}

