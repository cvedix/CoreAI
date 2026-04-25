#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtsp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## 1-1-1 sample ##
* 1 video input, 1 infer task, and 1 output.
*
* Output via RTSP (no X11 required):
*   View on macOS/Windows: vlc rtsp://<server-ip>:8554/live
*
* To use local screen display instead (requires X11):
*   Replace rtsp_des_0 with screen_des_0 (see commented code below).
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.6);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");

    // RTSP output — view with: vlc rtsp://<server-ip>:8554/live
    auto rtsp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_des_node>("rtsp_des_0", 0, 8554, "live");

    // Alternative: local screen display (requires X11, slow over SSH -X)
    // auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yunet_face_detector_0});
    rtsp_des_0->attach_to({osd_0});

    file_src_0->start();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  RTSP stream available at:" << std::endl;
    std::cout << "  rtsp://<server-ip>:8554/live" << std::endl;
    std::cout << "  Open with VLC on macOS/Windows" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
