#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_enet_seg_node.h"
#include "cvedix/nodes/osd/cvedix_seg_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## enet seg sample ##
* semantic segmentation based on ENet.
* 1 input, 2 outputs including orignal frame and mask frame.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/enet_seg.mp4");
    auto enet_seg = std::make_shared<cvedix_nodes::cvedix_enet_seg_node>("enet_seg", "./cvedix_data/models/enet-cityscapes/enet-model.net");
    auto seg_osd_0 = std::make_shared<cvedix_nodes::cvedix_seg_osd_node>("seg_osd_0", "./cvedix_data/models/enet-cityscapes/enet-classes.txt", "./cvedix_data/models/enet-cityscapes/enet-colors.txt");
    auto screen_des_mask = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_mask", 0, true, cvedix_objects::cvedix_size(400, 225));
    auto screen_des_original = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_original", 0, false, cvedix_objects::cvedix_size(400, 225));

    // construct pipeline
    enet_seg->attach_to({file_src_0});
    seg_osd_0->attach_to({enet_seg});
    screen_des_mask->attach_to({seg_osd_0});
    screen_des_original->attach_to({seg_osd_0});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}