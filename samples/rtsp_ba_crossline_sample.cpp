#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_ba_crossline_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## rtsp ba crossline sample ##
* behaviour analysis for crossline with RTSP input.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    // Using RTSP source from rtsp_src_sample.cpp
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>("rtsp_src_0", 0, "rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo", 0.6);
    
    // Using YOLO detector and tracker from ba_crossline_sample.cpp
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolo_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");
    
    // Create ba
    // define a line in frame for every channel (value MUST in the scope of frame'size)
    cvedix_objects::cvedix_point start(0, 250);  // change to proper value
    cvedix_objects::cvedix_point end(700, 220);  // change to proper value
    std::map<int, cvedix_objects::cvedix_line> lines = {{0, cvedix_objects::cvedix_line(start, end)}};  // channel0 -> line
    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_crossline_node>("ba_crossline", lines);
    
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_crossline_osd_node>("osd");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://anhoidong.datacenter.cvedix.com:1935/live/camera_traffic_usa_ai");
    
    // construct pipeline
    // Connect RTSP source to YOLO detector
    yolo_detector->attach_to({rtsp_src_0});
    tracker->attach_to({yolo_detector});
    ba_crossline->attach_to({tracker});
    osd->attach_to({ba_crossline});
    screen_des_0->attach_to({osd});
    rtmp_des_0->attach_to({osd});

    rtsp_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({rtsp_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    rtsp_src_0->detach_recursively();
}
