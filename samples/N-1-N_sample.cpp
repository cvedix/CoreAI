#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## N-1-N sample ##
* 2 video input and merge into 1 branch automatically for 1 infer task, then resume to 2 branches for outputs again.
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/face2.mp4", 0.6);
    auto yunet_face_detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);  // split by channel index
    
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://192.168.77.60/live/10000");

    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_1");
    auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);
    auto rtmp_des_1 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_1", 1, "rtmp://192.168.77.60/live/10000");

    // construct pipeline
    yunet_face_detector->attach_to({file_src_0, file_src_1});
    sface_face_encoder->attach_to({yunet_face_detector});
    
    split->attach_to({sface_face_encoder});

    // split by cvedix_split_node
    osd_0->attach_to({split});
    osd_1->attach_to({split});

    // auto split again on channel 0
    screen_des_0->attach_to({osd_0});
    rtmp_des_0->attach_to({osd_0});

    // auto split again on channel 1
    screen_des_1->attach_to({osd_1});
    rtmp_des_1->attach_to({osd_1});

    file_src_0->start();
    file_src_1->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0, file_src_1});
    board.display();
}