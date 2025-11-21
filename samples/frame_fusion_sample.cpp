#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/proc/cvedix_frame_fusion_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_placeholder_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## frame_fusion_sample ##
* fuse frames of 2 channels, just merge adjacent frames from 2 channels directly without considering timestamp synchronization.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/infrared.mp4");   // source of fusion
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/rgb.mp4");        // destination of fusion
    // initialize calibration points manually
    std::vector<cvedix_objects::cvedix_point> src_cali_points = {cvedix_objects::cvedix_point(133, 111), cvedix_objects::cvedix_point(338, 110), cvedix_objects::cvedix_point(15, 330), cvedix_objects::cvedix_point(14, 214)};
    std::vector<cvedix_objects::cvedix_point> des_cali_points = {cvedix_objects::cvedix_point(1219, 365), cvedix_objects::cvedix_point(1787, 367), cvedix_objects::cvedix_point(891, 982), cvedix_objects::cvedix_point(892, 659)};
    auto fusion = std::make_shared<cvedix_nodes::cvedix_frame_fusion_node>("fusion", src_cali_points, des_cali_points);
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);
    auto screen_des_0_ori = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_ori", 0, false);  // original frame for the first channel
    auto screen_des_1_osd = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1_osd", 1);         // fusion result(osd frame) for the second channel

    // construct pipeline
    fusion->attach_to({file_src_0, file_src_1});
    split->attach_to({fusion});
    screen_des_0_ori->attach_to({split});
    screen_des_1_osd->attach_to({split});

    file_src_0->start();
    file_src_1->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0, file_src_1});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    file_src_1->detach_recursively();
}