#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/broker/cvedix_sse_broker_node.h"


#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## 1-1-1 sample ##
* 1 video input, 1 infer task, and 1 broker.
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"sse_broker"});  // only write debug log for sync node
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto sse_broker_0 = std::make_shared<cvedix_nodes::cvedix_sse_broker_node>("sse_broker",cvedix_nodes::cvedix_broke_for::FACE,50,200,8090,"/events");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Customize output message (optional)
    sse_broker_0->set_json_transformer([](const std::string& msg) {
        return "{\"type\":\"detection\",\"payload\":" + msg + "}";
    });

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    sse_broker_0->attach_to({yunet_face_detector_0});
    osd_0->attach_to({sse_broker_0});
    screen_des_0->attach_to({osd_0});
    file_src_0->start();

    // for debug purpose
    // cvedix_utils::cvedix_analysis_board board({file_src_0});
    // board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
