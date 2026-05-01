#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## 1-1-N sample ##
* 1 video input, 1 infer task, and 1 output (switchable via --mode).
*
* Usage:
*   ./1-1-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.8);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");

    // create output destination based on --mode
    auto output = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yunet_face_detector_0});
    output.des_node->attach_to({osd_0});

    file_src_0->start();
    sample_helper::init_board(output);
    sample_helper::print_output_info(out_cfg);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
