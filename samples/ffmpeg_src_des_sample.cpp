#include "cvedix/nodes/ffio/cvedix_ff_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/ffio/cvedix_ff_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## ffmpeg_src_des_sample ##
* reading & pushing stream based on ffmpeg (soft decode and encode using CPUs).
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    /*
       uri for cvedix_ff_src_node:
       1. rtmp://192.168.77.196/live/1000        --> reading rtmp live stream
       2. rtsp://192.168.77.213/live/mainstream  --> reading rtsp live stream
       3. ./cvedix_data/test_video/face.mp4          --> reading video file

       uri for cvedix_ff_des_node:
       1. rtmp://192.168.77.196/live/10000       --> pushing rtmp live stream
       2. ./output/records.mp4                   --> saving to video file
    */
    auto ff_src_0 = std::make_shared<cvedix_nodes::cvedix_ff_src_node>("ff_src_0", 0, "./cvedix_data/test_video/face.mp4", "h264", 0.6);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    auto ff_des_0 = std::make_shared<cvedix_nodes::cvedix_ff_des_node>("ff_des_0", 0, "rtmp://192.168.77.60/live/20000");
    //auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://192.168.77.196/live/20000");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    yunet_face_detector_0->attach_to({ff_src_0});
    sface_face_encoder_0->attach_to({yunet_face_detector_0});
    osd_0->attach_to({sface_face_encoder_0});
    ff_des_0->attach_to({osd_0});
    screen_des_0->attach_to({osd_0});

    ff_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({ff_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    ff_src_0->detach_recursively();
}