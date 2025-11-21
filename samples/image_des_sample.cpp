#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_image_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## image_des_sample ##
* show how cvedix_image_des_node works, save image to local file or push image to remote via udp.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    
    /* save to file, `%d` is placeholder for filename index */
    //auto image_des_0 = std::make_shared<cvedix_nodes::cvedix_image_des_node>("image_file_des_0", 0, "./images/%d.jpg", 3, cvedix_objects::cvedix_size(), false);
    
    /* push via udp,  receiving command for test: `gst-launch-1.0 udpsrc port=6000 ! application/x-rtp,encoding-name=jpeg ! rtpjpegdepay ! jpegparse ! jpegdec ! queue ! videoconvert ! autovideosink` */
    auto image_des_0 = std::make_shared<cvedix_nodes::cvedix_image_des_node>("image_udp_des_0", 0, "192.168.77.248:6000", 2, cvedix_objects::cvedix_size(600, 300));

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    sface_face_encoder_0->attach_to({yunet_face_detector_0});
    osd_0->attach_to({sface_face_encoder_0});
    screen_des_0->attach_to({osd_0});
    image_des_0->attach_to({osd_0});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}