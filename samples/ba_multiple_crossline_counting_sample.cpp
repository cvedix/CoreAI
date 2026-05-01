#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_counting_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## ba multiple crossline counting sample ##
* behaviour analysis for crossline counting with multiple lines.
*
* Usage:
*   ./ba_multiple_crossline_counting_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_crossline"});
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/vehicle_count.mp4", 0.4);
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "yolo_detector",
      "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
      cvedix_nodes::YoloVersion::YOLO11,
      "./cvedix_data/models/yolov11/onnx/labels.txt",
      0.45, 0.5, 0, cvedix_nodes::BackendType::ONNX);

    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>("track_0", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

    // define lines
    cvedix_objects::cvedix_point start1(250, 250);
    cvedix_objects::cvedix_point end1(450, 250);
    cvedix_nodes::cvedix_ba_line_couting_setting line_setting1;
    line_setting1.setting_name = "line_1";
    line_setting1.line = cvedix_objects::cvedix_line(start1, end1);
    line_setting1.direction = cvedix_objects::cvedix_ba_direct_type::IN;

    cvedix_objects::cvedix_point start2(500, 250);
    cvedix_objects::cvedix_point end2(700, 250);
    cvedix_nodes::cvedix_ba_line_couting_setting line_setting2;
    line_setting2.setting_name = "line_2";
    line_setting2.line = cvedix_objects::cvedix_line(start2, end2);
    line_setting2.direction = cvedix_objects::cvedix_ba_direct_type::OUT;

    std::map<int, std::vector<cvedix_nodes::cvedix_ba_line_couting_setting>> line_settings = {
        {0, {line_setting1, line_setting2}},
    };

    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_counting_node>("ba_crossline", line_settings);
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

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
