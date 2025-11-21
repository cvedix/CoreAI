#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_face_swap_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## face_swap_sample ##
* swap face for any video/images, no training need before running.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 1.0, true, "avdec_h264", 4);
    auto yunet_face_detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto face_swap = std::make_shared<cvedix_nodes::cvedix_face_swap_node>("face_swap", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx", "./cvedix_data/models/face/swap/w600k_r50.onnx", "./cvedix_data/models/face/swap/emap.txt", "./cvedix_data/models/face/swap/inswapper_128.onnx", "./github/inswapper/data/mans1.jpeg");
    //auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd");
    auto screen_des_0_ori = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_ori", 0, false);
    auto screen_des_0_osd = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_osd", 0);
    auto file_des_node_0 = std::make_shared<cvedix_nodes::cvedix_file_des_node>("file_des_0", 0, ".", "", 1);

    // construct pipeline
    yunet_face_detector->attach_to({file_src_0});
    face_swap->attach_to({yunet_face_detector});
    //osd->attach_to({face_swap});
    screen_des_0_ori->attach_to({face_swap});
    screen_des_0_osd->attach_to({face_swap});
    file_des_node_0->attach_to({face_swap}); // save swap result to file

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}