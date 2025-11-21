#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_lane_detector_node.h"
#include "cvedix/nodes/osd/cvedix_lane_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## lane_detect_sample ##
* detect lanes on road.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.6, true, "avdec_h264", 4);
    auto lane_detector = std::make_shared<cvedix_nodes::cvedix_lane_detector_node>("lane_detector", "./cvedix_data/models/lane/lane_det.onnx");
    auto lane_osd = std::make_shared<cvedix_nodes::cvedix_lane_osd_node>("lane_osd");
    auto screen_des_0_osd = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0_osd", 0);    
    auto srceen_des_0_ori = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("srceen_des_0_ori", 0, false);

    // construct pipeline
    lane_detector->attach_to({file_src_0});
    lane_osd->attach_to({lane_detector});
    screen_des_0_osd->attach_to({lane_osd});
    srceen_des_0_ori->attach_to({lane_osd});

    // start pipeline
    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}