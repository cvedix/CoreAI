#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## web_debug_sample ##
* Demonstrates the Web Debug Dashboard — a browser-based alternative
* to screen_des_node and analysis_board.display().
*
* No X11 required! Open http://localhost:9090 in any browser.
*
* Pipeline:
*   file_src → face_detector → osd → web_debug_des
*
* Features:
*   - Live OSD video stream (MJPEG)
*   - Pipeline Analysis Board (MJPEG)
*   - Detection events (SSE)
*   - Real-time stats (JSON API)
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // 1. Create pipeline nodes
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/face.mp4", 0.6);

    auto face_detector = std::make_shared<cvedix_nodes::cvedix_face_detector_node>(
        "face_detector", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    // 2. Create web debug destination (serves dashboard at port 9090)
    //    Board pointer is set later after pipeline is fully connected.
    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, 9090, nullptr, 75);

    // 3. Build pipeline (must be done BEFORE creating analysis board)
    face_detector->attach_to({file_src});
    osd->attach_to({face_detector});
    web_debug->attach_to({osd});

    // 4. Create analysis board AFTER pipeline is fully connected
    //    (board validates pipeline must end with DES nodes)
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);  // render to buffer at 5fps (no GUI)
    web_debug->set_board(&board);

    // 5. Start
    file_src->start();

    std::cout << "\n"
              << "╔══════════════════════════════════════════════╗\n"
              << "║  OmniCore Web Debug Dashboard               ║\n"
              << "║                                              ║\n"
              << "║  Open in browser:                            ║\n"
              << "║  → http://localhost:9090                     ║\n"
              << "║                                              ║\n"
              << "║  Press Enter to stop...                      ║\n"
              << "╚══════════════════════════════════════════════╝\n"
              << std::endl;

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
}
