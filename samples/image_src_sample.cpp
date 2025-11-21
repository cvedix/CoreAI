#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## image_des_sample ##
* show how cvedix_image_src_node works, read image from local file or receive image from remote via udp.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_file_src_0", 0, "./cvedix_data/test_images/vehicle/%d.jpg", 1, 0.4); // read 1 image EVERY 1 second from local files, such as test_0.jpg,test_1.jpg
    /* sending command for test: `gst-launch-1.0 filesrc location=16.mp4 ! qtdemux ! avdec_h264 ! videoconvert ! videoscale ! video/x-raw,width=416,height=416 ! videorate ! video/x-raw,framerate=1/1 ! jpegenc ! rtpjpegpay ! udpsink host=ip port=6000` */
    auto image_src_1 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_udp_src_1", 1, "6000", 3);                       // receive 1 image EVERY 3 seconds from remote via udp , such as 127.0.0.1:6000
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolo_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split_by_channel", true);    // split by channel index (important!)
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);
    
    // construct pipeline
    yolo_detector->attach_to({image_src_0, image_src_1});
    osd->attach_to({yolo_detector});
    split->attach_to({osd});
    screen_des_0->attach_to({split});
    screen_des_1->attach_to({split});

    image_src_0->start();  // start read from local file
    image_src_1->start();  // start receive from remote via udp

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({image_src_0, image_src_1});
    board.display();
}