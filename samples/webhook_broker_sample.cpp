
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/broker/cvedix_webhook_broker_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
 * ## webhook broker sample ##
 * Demonstrates how to use cvedix_webhook_broker_node to HTTP POST
 * detection results as JSON to a webhook endpoint.
 *
 * To test, start a simple HTTP server on port 9999:
 *   python3 -c "
 *   from http.server import HTTPServer, BaseHTTPRequestHandler
 *   import json
 *   class H(BaseHTTPRequestHandler):
 *       def do_POST(self):
 *           data = self.rfile.read(int(self.headers['Content-Length']))
 *           print('Received:', json.loads(data))
 *           self.send_response(200)
 *           self.end_headers()
 *   HTTPServer(('0.0.0.0', 9999), H).serve_forever()
 *   "
 *
 * Then run this sample — detection results will be POSTed to localhost:9999.
 */

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // 1. Source: video file
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/face.mp4", 1.0);

    // 2. Detector: YuNet face detector
    auto detector = std::make_shared<cvedix_nodes::cvedix_face_detector_node>(
        "face_detector", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");

    // 3. Webhook broker: POST face detections as JSON
    auto webhook_broker = std::make_shared<cvedix_nodes::cvedix_webhook_broker_node>(
        "webhook_broker",
        "http://localhost:9999/detections",     // webhook URL
        cvedix_nodes::cvedix_broke_for::FACE,   // serialize face targets
        50,   // warn threshold
        200,  // ignore threshold
        // Optional: JSON transformer to wrap data
        [](const std::string& json) {
            return "{\"source\":\"face_sample\",\"payload\":" + json + "}";
        },
        // Optional: custom headers
        {{"X-Source", "omnisdk-sample"}, {"X-Version", "1.0"}},
        5,    // timeout seconds
        2     // retry count
    );

    // 4. OSD + display
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // Build pipeline: src → detector → webhook_broker → osd → screen
    detector->attach_to({file_src});
    webhook_broker->attach_to({detector});
    osd->attach_to({webhook_broker});
    screen->attach_to({osd});

    file_src->start();

    // Analysis board for debugging
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.display();
}
