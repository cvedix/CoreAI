#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_restoration_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## video_restoration_sample ##
* enhance for any video/images, no training need before running.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_file_src_0", 0, "./cvedix_data/test_images/restoration/1/%d.jpg", 3); 
    auto restoration_node = std::make_shared<cvedix_nodes::cvedix_restoration_node>("restoration_node", "./cvedix_data/models/restoration/realesrgan-x4.onnx");
    auto screen_des_0_ori = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_ori", 0, false);
    auto screen_des_0_osd = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_osd", 0);

    // construct pipeline
    restoration_node->attach_to({image_src_0});
    screen_des_0_ori->attach_to({restoration_node});
    screen_des_0_osd->attach_to({restoration_node});

    // start pipeline
    image_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({image_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    image_src_0->detach_recursively();
}