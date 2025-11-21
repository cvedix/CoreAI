#include <iostream>
#include <memory>
#include <string>

// CVEDIX SDK public headers
#include <cvedix/cvedix_version.h>
#include <cvedix/utils/cvedix_utils.h>
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>
#include <cvedix/nodes/src/cvedix_image_src_node.h>
#include <cvedix/nodes/infers/cvedix_yolo_detector_node.h>
#include <cvedix/nodes/osd/cvedix_osd_node.h>
#include <cvedix/nodes/des/cvedix_screen_des_node.h>

/**
 * Ví dụ: Xử lý ảnh bằng CVEDIX SDK
 *
 * Pipeline:
 *   Image Source -> YOLO Detector -> OSD -> Screen Display
 *
 * Yêu cầu:
 *   - Tải dataset/mô hình: cvedix_data (xem README)
 *   - Đặt biến môi trường CVEDIX_DATA_ROOT hoặc chỉnh sửa đường dẫn bên dưới
 *
 * Build & run:
 *   mkdir build && cd build
 *   cmake ..
 *   make
 *   ./example_using_sdk
 */

static std::string resolve_path(const std::string &relative) {
    const char *root = std::getenv("CVEDIX_DATA_ROOT");
    if (root == nullptr) {
        return "./cvedix_data/" + relative;
    }
    std::string base(root);
    if (!base.empty() && base.back() != '/') {
        base += '/';
    }
    return base + relative;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "CVEDIX Instance Pipeline SDK - Image Sample" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Version: " << CVEDIX_VERSION << std::endl;
    std::cout << "Build Time: " << CVEDIX_BUILD_TIME << std::endl;
    std::cout << "Git Commit: " << CVEDIX_GIT_COMMIT << std::endl;
    std::cout << std::endl;

    // Chuẩn bị đường dẫn dữ liệu/mô hình
    const std::string image_pattern = resolve_path("test_images/vehicle/%d.jpg");
    const std::string weights_path = resolve_path("models/det_cls/yolov3-tiny-2022-0721_best.weights");
    const std::string config_path = resolve_path("models/det_cls/yolov3-tiny-2022-0721.cfg");
    const std::string labels_path = resolve_path("models/det_cls/yolov3_tiny_5classes.txt");

    std::cout << "Image pattern: " << image_pattern << std::endl;
    std::cout << "Weights:       " << weights_path << std::endl;
    std::cout << "Config:        " << config_path << std::endl;
    std::cout << "Labels:        " << labels_path << std::endl;
    std::cout << std::endl;

    // Khởi tạo logger
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    try {
        // 1. Tạo các node trong pipeline
        auto image_src = std::make_shared<cvedix_nodes::cvedix_image_src_node>(
            "image_src",                // node name
            0,                           // channel index
            image_pattern,               // pattern của ảnh
            1,                           // đọc 1 ảnh mỗi giây
            0.4f                         // scale factor (tăng tốc xử lý)
        );

        auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            "yolo_detector",
            weights_path,
            config_path,
            labels_path
        );

        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_overlay");
        auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_output", 0);

        // 2. Kết nối pipeline
        yolo_detector->attach_to({image_src});
        osd->attach_to({yolo_detector});
        screen->attach_to({osd});

        // 3. Khởi động pipeline
        image_src->start();

        std::cout << "Pipeline started. OpenCV windows will display detection results." << std::endl;
        std::cout << "Press ENTER to stop..." << std::endl;

        // 4. Hiển thị bảng phân tích để debug / theo dõi pipeline
        cvedix_utils::cvedix_analysis_board board({image_src});
        board.display(1, false); // refresh mỗi 1s, không auto-close

        // 5. Chờ người dùng kết thúc
        std::string wait;
        std::getline(std::cin, wait);

        // 6. Giải phóng pipeline
        image_src->detach_recursively();
        std::cout << "Pipeline stopped." << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "[ERROR] " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

