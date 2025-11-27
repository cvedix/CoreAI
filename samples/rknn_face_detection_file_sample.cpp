#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_face_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>

/*
* ## Ví dụ phát hiện khuôn mặt sử dụng RKNN NPU với file video ##
* Đầu vào File Video → Bộ phát hiện RKNN Face Detector → Bộ theo dõi SORT → 
* OSD → Hiển thị màn hình / RTMP streaming
*
* Tính năng:
* - Phát hiện khuôn mặt sử dụng RKNN NPU (tối ưu cho Rockchip hardware)
* - Sử dụng model YuNet (libfacedetection) đã được convert sang RKNN format
* - Hỗ trợ phát hiện 5 keypoints (mắt trái, mắt phải, mũi, góc miệng trái, góc miệng phải)
* - Tự động sử dụng RGA (Rockchip Graphics Accelerator) nếu có để tăng tốc pre-processing
* - Theo dõi nhiều khuôn mặt cùng lúc
* - Đọc video từ file local (mp4, mkv, avi, etc.)
* - Hỗ trợ lặp lại video khi kết thúc (cycle mode)
*
* Yêu cầu:
* - Model RKNN (.rknn) cho face detection (YuNet format)
*   Model mặc định: ./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn
*   Có thể convert từ ONNX model: https://github.com/ShiqiYu/libfacedetection
* - File video để xử lý (mp4, mkv, avi, etc.)
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
*
* Sử dụng:
*   ./rknn_face_detection_file_sample [model_path] [video_file_path]
*
* Ví dụ:
*   # Sử dụng model và video path mặc định
*   ./rknn_face_detection_file_sample
*
*   # Chỉ định model path
*   ./rknn_face_detection_file_sample ./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn
*
*   # Chỉ định cả model và video file path
*   ./rknn_face_detection_file_sample ./model.rknn ./video/test.mp4
*/

// Cờ toàn cục để xử lý tín hiệu
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

int main(int argc, char** argv) {
    // Xử lý Ctrl+C
    std::signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Đường dẫn model mặc định - thử nhiều path có thể
    std::string model_path = "./cvedix_data/models/face/rk3588/face_detection_yunet_2022mar_int8.rknn";
    if (argc > 1) {
        model_path = argv[1];
    } else {
        // Thử fallback path nếu path mặc định không tồn tại
        std::ifstream test_file(model_path);
        if (!test_file.good()) {
            std::string fallback_path = "./cvedix_data/models/face/yunet_face_detection.rknn";
            std::ifstream fallback_test(fallback_path);
            if (fallback_test.good()) {
                model_path = fallback_path;
                std::cout << "[Main] Using fallback model path: " << model_path << std::endl;
            }
        }
    }
    
    // Kiểm tra file model có tồn tại không
    std::ifstream model_file(model_path);
    if (!model_file.good()) {
        std::cerr << "[Main] ERROR: Model file not found: " << model_path << std::endl;
        std::cerr << "[Main] Please provide correct model path as first argument:" << std::endl;
        std::cerr << "[Main]   ./rknn_face_detection_file_sample <model_path> [video_file_path]" << std::endl;
        std::cerr << "[Main] Example:" << std::endl;
        std::cerr << "[Main]   ./rknn_face_detection_file_sample ./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn ./video/test.mp4" << std::endl;
        return 1;
    }
    model_file.close();
    
    // Đường dẫn file video mặc định
    std::string video_file_path = "./cvedix_data/test_video/face.mp4";
    if (argc > 2) {
        video_file_path = argv[2];
    }
    
    // Kiểm tra file video có tồn tại không
    std::ifstream video_file(video_file_path);
    if (!video_file.good()) {
        std::cerr << "[Main] ERROR: Video file not found: " << video_file_path << std::endl;
        std::cerr << "[Main] Please provide correct video file path as second argument:" << std::endl;
        std::cerr << "[Main]   ./rknn_face_detection_file_sample <model_path> <video_file_path>" << std::endl;
        std::cerr << "[Main] Example:" << std::endl;
        std::cerr << "[Main]   ./rknn_face_detection_file_sample ./model.rknn ./video/test.mp4" << std::endl;
        return 1;
    }
    video_file.close();
    
    std::cout << "[Main] RKNN Face Detection Sample (File Source)" << std::endl;
    std::cout << "[Main] Model: " << model_path << std::endl;
    std::cout << "[Main] Video File: " << video_file_path << std::endl;

    // Đầu vào: Nguồn file video
    // resize_ratio: 1.0 để giữ nguyên độ phân giải gốc (tốt hơn cho detection)
    // cycle: true để lặp lại video khi kết thúc
    // gst_decoder_name: "mppvideodec" để sử dụng hardware decoder (Rockchip MPP)
    //   Có thể dùng "avdec_h264" cho software decoder nếu hardware decoder không khả dụng
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 
        0, 
        video_file_path, 
        1.0,        // resize_ratio
        true,       // cycle (lặp lại video)
        "mppvideodec",  // gst_decoder_name (hardware decoder)
        0           // skip_interval (0 = không bỏ qua frame nào)
    );
    
    // Suy luận: Phát hiện khuôn mặt sử dụng RKNN NPU
    // Tối ưu hóa cho face detection:
    // - Input size: 320x320 (tốt cho face detection, cân bằng giữa tốc độ và độ chính xác)
    //   Có thể tăng lên 640x640 để tăng độ chính xác nhưng sẽ chậm hơn
    // - Score threshold: 0.7 (ngưỡng điểm để lọc false positives)
    // - NMS threshold: 0.5 (ngưỡng NMS để loại bỏ overlapping boxes)
    // - Top K: 50 (số lượng khuôn mặt tối đa có thể phát hiện)
    auto rknn_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_face_detector_node>(
        "rknn_face_detector_0", 
        model_path,
        0.2f,   // score threshold (giảm xuống để dễ phát hiện hơn, có thể tăng lên 0.5-0.7 nếu có nhiều false positives)
        0.5f,   // nms threshold
        50,     // top_k
        320,    // input width (có thể tăng lên 640 để tăng độ chính xác)
        320     // input height
    );

    // Theo dõi: Bộ theo dõi SORT cho khuôn mặt
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::FACE  // Track cho face
    );
    
    // OSD: Vẽ kết quả lên khung hình (vẽ bounding box và keypoints)
    auto face_osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("face_osd_0");

    // Đầu ra: Hiển thị lên màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Đầu ra: Gửi lên RTMP server (optional)
    // auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
    //     "rtmp_des_0", 0, "rtmp://...", cvedix_objects::cvedix_size{1280, 720}, 1024 * 2);

    // Xây dựng pipeline
    rknn_face_detector_0->attach_to({file_src_0});
    sort_tracker_0->attach_to({rknn_face_detector_0});
    face_osd_0->attach_to({sort_tracker_0});
    screen_des_0->attach_to({face_osd_0});
    // rtmp_des_0->attach_to({face_osd_0});  // Uncomment nếu muốn stream RTMP

    // Khởi động pipeline
    file_src_0->start();

    // Bảng phân tích (GUI) - hiển thị thống kê performance
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Dọn dẹp
    file_src_0->detach_recursively();
    
    std::cout << "[Main] Shutting down..." << std::endl;
    
    return 0;
}

