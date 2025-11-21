#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_jam_node.h"
#include "cvedix/nodes/osd/cvedix_ba_jam_osd_node.h"
#include "cvedix/nodes/record/cvedix_record_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## ba jam sample ##
* behaviour analysis for jam, single instance of ba node work on 2 channels.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/jam.mp4", 0.5);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/jam2.mp4");
    auto trt_vehicle_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>("vehicle_detector", "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt");
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");
    
    // define a region in frame for every channel (value MUST in the scope of frame'size)
    std::map<int, std::vector<cvedix_objects::cvedix_point>> regions = {
        {0, std::vector<cvedix_objects::cvedix_point>{cvedix_objects::cvedix_point(20, 360), 
                                            cvedix_objects::cvedix_point(400, 250), 
                                            cvedix_objects::cvedix_point(535, 250), 
                                            cvedix_objects::cvedix_point(555, 560), 
                                            cvedix_objects::cvedix_point(30, 550)}},  // channel0 -> region
        {1, std::vector<cvedix_objects::cvedix_point>{cvedix_objects::cvedix_point(20*0.6, 588*0.6), 
                                            cvedix_objects::cvedix_point(786*0.6, 180*0.6), 
                                            cvedix_objects::cvedix_point(968*0.6, 166*0.6), 
                                            cvedix_objects::cvedix_point(1220*0.6, 665*0.6)}} // channel1 -> region
                                            };
    auto ba_jam = std::make_shared<cvedix_nodes::cvedix_ba_jam_node>("ba_jam", regions);
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_jam_osd_node>("jam_osd");
    auto recorder = std::make_shared<cvedix_nodes::cvedix_record_node>("recorder", "./record", "./record");
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);
    
    // construct pipeline
    trt_vehicle_detector->attach_to({file_src_0, file_src_1});
    tracker->attach_to({trt_vehicle_detector});
    ba_jam->attach_to({tracker});
    osd->attach_to({ba_jam});
    recorder->attach_to({osd});
    split->attach_to({recorder});
    screen_des_0->attach_to({split});
    screen_des_1->attach_to({split});

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