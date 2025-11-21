#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_app_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## app_des_sample ##
* 1. reading video from file
* 2. detect faces and draw results
* 3. display on screen in host code using cv::imshow(...)
* using cvedix_app_des_node INSTEAD OF cvedix_screen_des_node for displaying.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4");
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd_0");
    auto app_des_0 = std::make_shared<cvedix_nodes::cvedix_app_des_node>("app_des_0", 0);

    // register callback to display result
    std::string ori_win_title = "original frame using cv::imshow(...)";
    std::string osd_win_title = "osd frame using cv::imshow(...)";
    cv::namedWindow(ori_win_title,cv::WindowFlags::WINDOW_NORMAL);
    cv::namedWindow(osd_win_title,cv::WindowFlags::WINDOW_NORMAL);
    app_des_0->set_app_des_result_hooker([&](std::string node_name, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        // only deal with frame meta
        if (meta->meta_type == cvedix_objects::cvedix_meta_type::FRAME) {
            auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
            cv::imshow(ori_win_title, frame_meta->frame);

            // osd frame may be empty
            if (!frame_meta->osd_frame.empty()) {
                cv::imshow(osd_win_title, frame_meta->osd_frame);
            }
        }
    });

    // construct pipeline
    yunet_face_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yunet_face_detector_0});
    app_des_0->attach_to({osd_0});

    // start pipeline
    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}