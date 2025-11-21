#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## skip_sample ##
* 2 inputs , and skip 2 frames every 3 frames for the 2nd channel.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.5);
    auto rtsp_src_1 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>("rtsp_src_1", 1, "rtsp://admin:admin12345@192.168.3.157", 0.4, "avdec_h264", 2);  // skip 2 frames every 3 frames
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolo_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto rtmp_des_1 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 1, "rtmp://192.168.77.60/live/10000");

    // construct pipeline
    yolo_detector->attach_to({file_src_0, rtsp_src_1});
    osd->attach_to({yolo_detector});
    split->attach_to({osd});
    screen_des_0->attach_to({split});
    rtmp_des_1->attach_to({split});

    // start channels
    file_src_0->start();
    rtsp_src_1->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0, rtsp_src_1});
    board.display();
}