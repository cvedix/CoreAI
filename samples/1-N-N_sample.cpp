#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## 1-N-N sample ##
* 1 video input and then split into 2 branches for different infer tasks, then 2 total outputs(no need to sync in such situations).
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", false, true);  // split by deep-copy not by channel!

    // branch a
    auto yunet_face_detector_a = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_a", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder_a = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_a", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto osd_a = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_a");
    auto screen_des_a = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_a", 0);

    // branch b
    auto yunet_face_detector_b = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_b", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder_b = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_b", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto osd_b = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_b");
    auto screen_des_b = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_b", 0);

    // construct pipeline
    split->attach_to({file_src_0});

    // branch a
    yunet_face_detector_a->attach_to({split});
    sface_face_encoder_a->attach_to({yunet_face_detector_a});
    osd_a->attach_to({sface_face_encoder_a});
    screen_des_a->attach_to({osd_a});

    // branch b
    yunet_face_detector_b->attach_to({split});
    sface_face_encoder_b->attach_to({yunet_face_detector_b});
    osd_b->attach_to({sface_face_encoder_b});
    screen_des_b->attach_to({osd_b});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}