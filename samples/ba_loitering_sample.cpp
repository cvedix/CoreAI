#include "cvedix/nodes/src/cvedix_file_src_node.h"
// #include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_stop_node.h"
#include "cvedix/nodes/ba/cvedix_ba_loitering_node.h"
#include "cvedix/nodes/osd/cvedix_ba_stop_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## ba stop sample ##
* behaviour analysis for stop, single instance of ba node work on 2 channels.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_stop.mp4", 0.6);
    // auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/vehicle_stop.mp4", 0.6);
    // auto trt_vehicle_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>("vehicle_detector", "./cvedix_data//models/trt/vehicle/vehicle_v8.5.trt");
    auto vehicle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolo_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");
    
    // define a region in frame for every channel (value MUST in the scope of frame'size)
    // Polygon ROI per channel
    std::map<int, std::vector<cvedix_objects::cvedix_point>> regions = {
        {0, std::vector<cvedix_objects::cvedix_point>{
            cvedix_objects::cvedix_point(20, 50),
            cvedix_objects::cvedix_point(600, 30),
            cvedix_objects::cvedix_point(700, 200),
            cvedix_objects::cvedix_point(600, 300),
            cvedix_objects::cvedix_point(20, 300)
        }}
    };

    // Loitering configuration per channel
    std::map<int, cvedix_nodes::loitering_config> configs = {
        {0, cvedix_nodes::loitering_config(5.0, "Parking Lot")} // alarm after 5s
    };

    auto ba_stop = std::make_shared<cvedix_nodes::cvedix_ba_loitering_node>("ba_stop", regions, configs, 30);

    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_stop_osd_node>("osd");
    // auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    // auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);
    
    // construct pipeline
    vehicle_detector->attach_to({file_src_0});
    tracker->attach_to({vehicle_detector});
    ba_stop->attach_to({tracker});
    osd->attach_to({ba_stop});
    // split->attach_to({osd});
    screen_des_0->attach_to({osd});
    // screen_des_1->attach_to({split});

    file_src_0->start();
    // file_src_1->start();

    // for debug purpose
    // cvedix_utils::cvedix_analysis_board board({file_src_0});
    // board.display();

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}