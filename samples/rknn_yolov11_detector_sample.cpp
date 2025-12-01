#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov11_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

/*
 * ## RKNN YOLOv11 Detector Sample ##
 * 
 * Mẫu code sử dụng cvedix_rknn_yolov11_detector_node để phát hiện đối tượng
 * từ video sử dụng mô hình RKNN YOLOv11.
 * 
 * Pipeline: 1 video input → 1 RKNN YOLOv11 detector → OSD → 1 output (màn hình)
 * 
 * Yêu cầu:
 * - Model RKNN (.rknn) đã được chuyển đổi từ YOLOv11
 * - librknnrt.so (bắt buộc)
 * - librga.so (tùy chọn, để tăng tốc preprocessing)
 * 
 * Build:
 *   cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
 *   make
 * 
 * Sử dụng:
 *   ./rknn_yolov11_detector_sample [model_path] [video_path] [labels_path]
 * 
 * Ví dụ:
 *   ./rknn_yolov11_detector_sample ./models/yolov11n.rknn ./test_video.mp4 ./labels.txt
 */

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [model_path] [video_path] [labels_path]" << std::endl;
    std::cout << "  model_path  : Đường dẫn đến file model RKNN (.rknn)" << std::endl;
    std::cout << "  video_path  : Đường dẫn đến file video đầu vào" << std::endl;
    std::cout << "  labels_path : Đường dẫn đến file labels (tùy chọn)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " ./cvedix_data/models/yolov11s.rknn ./cvedix_data/test_video/face_person.mp4 ./cvedix_data/models/det_cls/coco_labels.txt" << std::endl;
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse command line arguments
    std::string model_path = "./cvedix_data/models/rknn/rk3588/yolov11s.rknn";
    std::string video_path = "./cvedix_data/test_video/plate.mp4";
    std::string labels_path = "";  // Optional labels file
    
    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        model_path = argv[1];
    }
    
    if (argc > 2) {
        video_path = argv[2];
    }
    
    if (argc > 3) {
        labels_path = argv[3];
    }

    CVEDIX_INFO("=== RKNN YOLOv11 Detector Sample ===");
    CVEDIX_INFO("Model path: " + model_path);
    CVEDIX_INFO("Video path: " + video_path);
    if (!labels_path.empty()) {
        CVEDIX_INFO("Labels path: " + labels_path);
    } else {
        CVEDIX_INFO("Labels path: (not specified, using default class IDs)");
    }

    // Tạo node nguồn: Đọc video từ file
    // Tham số: node_name, channel_index, file_path, resize_ratio, cycle
    // resize_ratio: Tỷ lệ resize frame (0.0 < resize_ratio <= 1.0), 1.0 = không resize
    // cycle: true = lặp lại video vô hạn khi kết thúc (mặc định)
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 
        0, 
        video_path, 
        1.0f,  // resize_ratio = 1.0: không resize frame
        true   // cycle = true: lặp lại video vô hạn khi kết thúc
    );
    
    // Tạo node detector: Phát hiện đối tượng sử dụng RKNN YOLOv11
    // Tham số:
    //   - node_name: Tên node
    //   - model_path: Đường dẫn đến file model RKNN
    //   - score_threshold: Ngưỡng điểm số (0.0 - 1.0)
    //   - nms_threshold: Ngưỡng NMS (0.0 - 1.0)
    //   - input_width: Chiều rộng đầu vào của model
    //   - input_height: Chiều cao đầu vào của model
    //   - num_classes: Số lượng lớp đối tượng (mặc định 80 cho COCO)
    //   - labels_path: Đường dẫn đến file labels (tùy chọn)
    //   - class_id_offset: Offset cho class ID (mặc định 0)
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov11_detector_node>(
        "rknn_detector_0",
        model_path,
        0.5f,   // score_threshold: Chỉ giữ lại detections có confidence >= 0.5
        0.45f,  // nms_threshold: Ngưỡng NMS để loại bỏ overlapping boxes
        640,    // input_width: Chiều rộng đầu vào (model YOLOv11 thường dùng 640)
        640,    // input_height: Chiều cao đầu vào
        80,     // num_classes: Số lớp COCO (có thể thay đổi tùy model)
        labels_path,  // labels_path: File chứa tên các lớp (mỗi dòng một lớp)
        0       // class_id_offset: Offset cho class ID (dùng khi có nhiều detector)
    );
    
    // Tạo node OSD: Vẽ kết quả phát hiện lên frame
    // Node này sẽ vẽ bounding boxes, labels và scores lên frame
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    
    // Tạo node output: Hiển thị kết quả lên màn hình
    // Tham số: node_name, channel_index
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Xây dựng pipeline: Kết nối các node với nhau
    // Luồng xử lý: file_src → detector → osd → screen
    rknn_detector_0->attach_to({file_src_0});
    osd_0->attach_to({rknn_detector_0});
    screen_des_0->attach_to({osd_0});

    CVEDIX_INFO("Pipeline đã được xây dựng. Bắt đầu xử lý...");
    
    // Khởi động pipeline: Bắt đầu đọc và xử lý video
    file_src_0->start();

    // Tạo analysis board để hiển thị thông tin debug (GUI)
    // Tham số: danh sách các node cần theo dõi
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    CVEDIX_INFO("Pipeline đang chạy. Nhấn Enter để dừng...");

    // Chờ người dùng nhấn Enter để dừng pipeline
    std::string wait;
    std::getline(std::cin, wait);
    
    // Dừng và giải phóng tất cả các node trong pipeline
    file_src_0->detach_recursively();
    
    CVEDIX_INFO("Pipeline đã được dừng. Kết thúc chương trình.");
    
    return 0;
}

