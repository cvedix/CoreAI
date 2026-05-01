#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/track/cvedix_ocsort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## ba crossline sample ##
* behaviour analysis for crossline.
*
* Usage:
*   ./ba_crossline_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/vehicle_count.mp4", 0.4);
    
    // Create generic YOLO detector with backend plugin (TensorRT or OpenVINO)
    // For TensorRT backend:
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",                              // node name
        "./cvedix_data/models/yolov26/tensorrt/yolo26n.engine", // engine model file
        cvedix_nodes::YoloVersion::YOLO26,                      // use YOLOv26 plugin family
        "./cvedix_data/models/yolov26/tensorrt/labels.txt",  // labels file
        0.45,   // confidence threshold
        0.5,     // NMS threshold
        0,
        cvedix_nodes::BackendType::TENSORRT
    );
    
    // Optional: Configure detector
    // yolo_detector->set_conf_threshold(0.5);
    // yolo_detector->set_nms_threshold(0.45);
    
    // auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");
    // auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>("track_0", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15); 
    auto tracker = std::make_shared<cvedix_nodes::cvedix_ocsort_track_node>("track_0", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 15, 3, 0.3, 3, "iou", 0.2, true); 

    // define a line in frame for every channel (value MUST in the scope of frame'size)
    cvedix_objects::cvedix_point start(0, 250);  // change to proper value
    cvedix_objects::cvedix_point end(700, 220);  // change to proper value
    std::map<int, cvedix_objects::cvedix_line> lines = {{0, cvedix_objects::cvedix_line(start, end)}};  // channel0 -> line
    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>("ba_crossline", lines);
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = true;
    osd_cfg.show_track_trail = true;
    osd_cfg.show_center_dot = true;
    osd_cfg.show_static_zones = true;
    osd_cfg.enable_ba_enter_exit = false;
    osd_cfg.enable_ba_crossline = true;
    osd_cfg.enable_ba_crowding = false;
    osd_cfg.enable_ba_jam = false;
    osd_cfg.enable_ba_stop = false;
    osd_cfg.label_font_scale = 0.5;
    osd->update_config(osd_cfg);

    // create output destination based on --mode
    auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});
    
    // construct pipeline
    yolo_detector->attach_to({file_src_0});
    tracker->attach_to({yolo_detector});
    ba_crossline->attach_to({tracker});
    osd->attach_to({ba_crossline});
    output.des_node->attach_to({osd});

    file_src_0->start();

    sample_helper::init_board(output);
    sample_helper::print_output_info(out_cfg);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
