#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## face tracking sample ##
* track for multi faces using cvedix_sort_track_node.
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4");
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("track_0", cvedix_nodes::cvedix_track_for::FACE);   // track for face
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    sface_face_encoder_0->attach_to({yunet_face_detector_0});
    track_0->attach_to({sface_face_encoder_0});
    osd_0->attach_to({track_0});
    screen_des_0->attach_to({osd_0});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}