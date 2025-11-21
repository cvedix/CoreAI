#include "cvedix/nodes/src/cvedix_app_src_node.h"
#include "cvedix/nodes/infers/cvedix_ppocr_text_detector_node.h"
#include "cvedix/nodes/osd/cvedix_text_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## app src sample ##
* receive frames(cv::Mat) from host code
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto app_src_0 = std::make_shared<cvedix_nodes::cvedix_app_src_node>("app_src_0", 0);
    auto ppocr_text_detector = std::make_shared<cvedix_nodes::cvedix_ppocr_text_detector_node>("ppocr_text_detector", 
                                "./cvedix_data/models/text/ppocr/ch_PP-OCRv3_det_infer",
                                "./cvedix_data/models/text/ppocr/ch_ppocr_mobile_v2.0_cls_infer",
                                "./cvedix_data/models/text/ppocr/ch_PP-OCRv3_rec_infer",
                                "./cvedix_data/models/text/ppocr/ppocr_keys_v1.txt");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_text_osd_node>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    ppocr_text_detector->attach_to({app_src_0});
    osd_0->attach_to({ppocr_text_detector});
    screen_des_0->attach_to({osd_0});

    // start pipeline
    app_src_0->start();

    // visualize pipeline for debug
    cvedix_utils::cvedix_analysis_board board({app_src_0});
    board.display(1, false); // no block since we need interactions from console later

    // simulate push frame to pipeline regularly in a separate thread
    bool exit = false;
    auto simulate_run = [&]() {
        auto index = 0;
        auto count = 0;
        auto path = "./cvedix_data/test_images/text/";
        while (!exit) {
            auto frame = cv::imread(path + std::to_string(index) + ".jpg");
            assert(!frame.empty());
            
            app_src_0->push_frames({frame});  // push frame to pipeline, return false means failed
            count++;
            std::cout << "main thread has pushed [" << count << "] frames into pipeline..." << std::endl;
            
            index++;
            index = index % 3;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));  // sleep for 2 seconds
        }
    };
    std::thread simulate_thread(simulate_run);

    /* interact from console, start or stop the pipeline */
    /* no except check */
    std::string input;
    std::getline(std::cin, input);
    while (input != "quit") {
        if (input == "start") {
            app_src_0->start();
        }
        else if (input == "stop") {
            app_src_0->stop();  // app_src_0->push_frames(...) will print Warn message since it has stopped working
        }
        else {
            std::cout << "invalid command!" << std::endl;
        }
        std::getline(std::cin, input);
    }

    std::cout << "app_src_sample sample exits..." << std::endl;
    exit = true;
    simulate_thread.join();
    app_src_0->detach_recursively();
}