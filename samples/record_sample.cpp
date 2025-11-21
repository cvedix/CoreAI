#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/record/cvedix_record_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## record sample ##
* show how to use cvedix_record_node to record image and video.
* NOTE:
* the recording signal in this demo is triggered by users outside pipe (via calling cvedix_src_node::record_video_manually or cvedix_src_node::record_image_manually)
* in product situations, recording signal is triggered inside pipe automatically.
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_TO_CONSOLE(false);   // need interact on console
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/face2.mp4", 0.6);
    auto yunet_face_detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    auto sface_face_encoder = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    auto track = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("track", cvedix_nodes::cvedix_track_for::FACE);
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd");    
    auto recorder = std::make_shared<cvedix_nodes::cvedix_record_node>("recorder", "./record", "./record");

    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);  // split by channel index
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);

    // construct pipeline
    yunet_face_detector->attach_to({file_src_0, file_src_1});
    sface_face_encoder->attach_to({yunet_face_detector});
    track->attach_to({sface_face_encoder});
    osd->attach_to({track});
    recorder->attach_to({osd});
    split->attach_to({recorder});
    // split by cvedix_split_node
    screen_des_0->attach_to({split});
    screen_des_1->attach_to({split});

    /*
    * set hookers for cvedix_record_node when task compeleted
    */
    // define hooker 
    auto record_hooker = [](int channel, cvedix_nodes::cvedix_record_info record_info) {
        auto record_type = record_info.record_type == cvedix_nodes::cvedix_record_type::IMAGE ? "image" : "video";

        std::cout << "channel:[" << channel << "] [" <<  record_type << "]" <<  " record task completed! full path: " << record_info.full_record_path << std::endl;
    };
    recorder->set_image_record_complete_hooker(record_hooker);
    recorder->set_video_record_complete_hooker(record_hooker);

    // start channels
    file_src_0->start();
    file_src_1->start();

    // for debug purpose
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> src_nodes_in_pipe{file_src_0, file_src_1};
    cvedix_utils::cvedix_analysis_board board(src_nodes_in_pipe);
    board.display(1, false);  // no block

    
    /* interact from console */
    /* no except check */
    std::string input;
    std::getline(std::cin, input);
    // input format: `image channel` or `video channel`, like `video 0` means start recording video at channel 0
    auto inputs = cvedix_utils::string_split(input, ' '); 
    while (inputs[0] != "quit") {
        // no except check
        auto command = inputs[0];
        auto index = std::stoi(inputs[1]);
        auto src_by_channel = std::dynamic_pointer_cast<cvedix_nodes::cvedix_src_node>(src_nodes_in_pipe[index]);
        if (command == "video") {
            src_by_channel->record_video_manually(true);   // debug api
            // or
            // src_by_channel->record_video_manually(true, 5);
            // src_by_channel->record_video_manually(false, 20);
        }
        else if (command == "image") {
            src_by_channel->record_image_manually();   // debug api
            // or
            // src_by_channel->record_image_manually(true);
        }
        else {
            std::cout << "invalid command!" << std::endl;
        }
        std::getline(std::cin, input);
        inputs = cvedix_utils::string_split(input, ' '); 
        if (inputs.size() != 2) {
             std::cout << "invalid input!" << std::endl;
             break;
        }
    }

    std::cout << "record sample exits..." << std::endl;
    file_src_0->detach_recursively();
    file_src_1->detach_recursively();
}