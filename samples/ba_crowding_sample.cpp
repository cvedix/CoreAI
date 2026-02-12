#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_crowding_node.h"
#include "cvedix/nodes/osd/cvedix_ba_crowding_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

/*
* ## ba crowding sample ##
* behaviour analysis for crowding: detects when number of objects inside a
* configured ROI reaches threshold and have been inside for configured seconds.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/jam.mp4");
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "detector",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
      "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
      "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");

    // define polygon ROI per channel (list of points in frame coords)
    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois = {
        {0, std::vector<cvedix_objects::cvedix_point>{
            cvedix_objects::cvedix_point(20, 360), 
            cvedix_objects::cvedix_point(400, 250), 
            cvedix_objects::cvedix_point(700, 250), 
            cvedix_objects::cvedix_point(700, 700), 
            cvedix_objects::cvedix_point(30, 700)}
        }
    };

    // crowding configuration per channel
    std::map<int, cvedix_nodes::crowding_config> configs = {
        {0, cvedix_nodes::crowding_config(3, 2.0, "Lobby Area")} // threshold=3, alarm=2s
    };

    auto ba_crowding = std::make_shared<cvedix_nodes::cvedix_ba_crowding_node>("ba_crowding", rois, configs, 30, false, false);
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_crowding_osd_node>("crowding_osd");
    // auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);\

    // Optional use rtmp
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://127.0.0.1/live/9000");


    // construct pipeline
    detector->attach_to({file_src_0});
    tracker->attach_to({detector});
    ba_crowding->attach_to({tracker});
    osd->attach_to({ba_crowding});
    rtmp_des_0->attach_to({osd});

    file_src_0->start();

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
