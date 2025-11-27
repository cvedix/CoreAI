#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_yunet_int8_face_detection_mode.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
 * ## face_yunet_int8_sample ##
 *
 * Simple pipeline that runs the quantized YuNet INT8 model (FaceDetectorYN backend) on a video file.
 *
 * Default assets:
 *   Video : ./cvedix_data/test_video/face.mp4
 *   Model : ./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx
 */

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, "./cvedix_data/test_video/face.mp4");

    auto face_detector_0 = std::make_shared<cvedix_nodes::cvedix_face_yunet_int8_face_detection_mode>(
        "yunet_face_int8_0",
        "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
        0.8f,
        0.3f,
        5000,
        320,
        320);

    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_track_0", cvedix_nodes::cvedix_track_for::FACE);
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    face_detector_0->attach_to({file_src_0});
    track_0->attach_to({face_detector_0});
    osd_0->attach_to({track_0});
    screen_des_0->attach_to({osd_0});

    file_src_0->start();

    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();

    return 0;
}

