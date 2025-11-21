#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_openpose_detector_node.h"
#include "cvedix/nodes/osd/cvedix_pose_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## openpose sample ##
* pose estimation by OpenPose network.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/pose.mp4");
    auto openpose_detector = std::make_shared<cvedix_nodes::cvedix_openpose_detector_node>("openpose_detector", "./cvedix_data/models/openpose/pose/body_25_pose_iter_584000.caffemodel", "./cvedix_data/models/openpose/pose/body_25_pose_deploy.prototxt", "", 368, 368, 1, 0, 0.1, cvedix_objects::cvedix_pose_type::body_25);
    auto pose_osd_0 = std::make_shared<cvedix_nodes::cvedix_pose_osd_node>("pose_osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    openpose_detector->attach_to({file_src_0});
    pose_osd_0->attach_to({openpose_detector});
    screen_des_0->attach_to({pose_osd_0});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}