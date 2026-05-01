#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_area_crowding_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"

#include "sample_output_helper.h"

/*
* ## ba crowding sample ##
* behaviour analysis for crowding: detects when number of objects inside a
* configured ROI reaches threshold and have been inside for configured seconds.
*
* Usage:
*   ./ba_crowding_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/jam.mp4");
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",                              // node name
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx", // ONNX engine file
        cvedix_nodes::YoloVersion::YOLO11,                      // use YOLOv11 plugin family
        "./cvedix_data/models/yolov11/onnx/labels.txt",  // labels file
        0.45,   // confidence threshold
        0.5,     // NMS threshold
        0,
        cvedix_nodes::BackendType::ONNX
    );
    
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

    auto ba_crowding = std::make_shared<cvedix_nodes::cvedix_ba_area_crowding_node>("ba_crowding", rois, configs, 30, false, false);
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("crowding_osd");

    // create output destination based on --mode
    auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // construct pipeline
    detector->attach_to({file_src_0});
    tracker->attach_to({detector});
    ba_crowding->attach_to({tracker});
    osd->attach_to({ba_crowding});
    output.des_node->attach_to({osd});

    file_src_0->start();

    sample_helper::init_board(output);
    sample_helper::print_output_info(out_cfg);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
