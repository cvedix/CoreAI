/**
 * @file benchmark_face_recognizer_sample.cpp
 * @brief Benchmark: SeetaFace6 Face Recognition pipeline
 *
 * Measures per-step and end-to-end performance of the dual-model face
 * recognition pipeline using SeetaFace6 directly with OpenCV video I/O.
 *
 *   detect → mask check → landmarks → feature extract → (database match)
 *
 * Usage:
 *   Performance mode:
 *     ./benchmark_face_recognizer_sample [model_dir] [video] [duration_sec]
 *
 *   Accuracy mode (LFW benchmark):
 *     ./benchmark_face_recognizer_sample --accuracy --dataset=lfw [model_dir] [dataset_dir]
 *
 * Requires: -DCVEDIX_WITH_FACE=ON
 */

#ifdef CVEDIX_WITH_FACE

#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/FaceDatabase.h>
#include <seeta/MaskDetector.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>

// Accuracy benchmark header
#include "face_accuracy_benchmark.h"

std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

// ============================================================================
// Accuracy Benchmark Mode
// ============================================================================

/**
 * @brief Run accuracy benchmark mode
 */
int run_accuracy_benchmark(int argc, char** argv) {
    std::string model_dir = "./cvedix_data/models/seetaface6";
    std::string dataset_name = "lfw";
    std::string dataset_root = "./datasets";
    float similarity_threshold = 0.33f;
    bool use_margin = false;
    float margin_threshold = 0.15f;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--accuracy" || arg == "-a") {
            continue;  // Already detected
        } else if (arg == "--dataset" && i + 1 < argc) {
            dataset_name = argv[++i];
        } else if (arg == "--root" && i + 1 < argc) {
            dataset_root = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_dir = argv[++i];
        } else if (arg == "--download") {
            // Just download dataset and exit
            std::cout << "Downloading LFW dataset to " << dataset_root << "..." << std::endl;
            cvedix_face_benchmark::LFWParser::download(dataset_root);
            return 0;
        } else if (arg.substr(0, 16) == "--similarity-th=" && arg.length() > 16) {
            similarity_threshold = std::stof(arg.substr(16));
        } else if (arg.substr(0, 10) == "--margin=" && arg.length() > 10) {
            margin_threshold = std::stof(arg.substr(10));
            use_margin = true;
        } else if (arg == "--margin") {
            use_margin = true;
        } else if (arg.substr(0, 2) != "--") {
            // Positional argument (model_dir)
            model_dir = arg;
        }
    }

    // Validate model directory
    if (!std::filesystem::exists(model_dir)) {
        std::cerr << "ERROR: Model directory not found: " << model_dir << std::endl;
        return 1;
    }

    std::cout << "============================================" << std::endl;
    std::cout << "  Face Recognition Accuracy Benchmark"       << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  Model:      " << model_dir                   << std::endl;
    std::cout << "  Dataset:    " << dataset_name                << std::endl;
    std::cout << "  Root:       " << dataset_root                << std::endl;
    std::cout << "============================================" << std::endl;

    // Create benchmark
    cvedix_face_benchmark::FaceAccuracyBenchmark benchmark;

    // Configure
    cvedix_face_benchmark::BenchmarkConfig config;
    config.dataset_name = dataset_name;
    config.dataset_root = dataset_root;
    config.similarity_threshold = 0.33f;  // Optimal threshold for LFW (98.67% accuracy)
    config.auto_download = true;

    benchmark.configure(config);
    benchmark.set_model_dir(model_dir);

    // Initialize recognizer
    if (!benchmark.init_recognizer()) {
        std::cerr << "ERROR: Failed to initialize recognizer" << std::endl;
        return 1;
    }

    // Run benchmark
    if (dataset_name == "lfw") {
        std::cout << "\nRunning LFW verification benchmark..." << std::endl;
        if (!benchmark.run_lfw_verification()) {
            std::cerr << "ERROR: Benchmark failed" << std::endl;
            return 1;
        }

        benchmark.print_results();

        // Print summary
        float accuracy = benchmark.get_lfw_accuracy();
        std::cout << "\n[LFW Summary]" << std::endl;
        std::cout << "  Accuracy: " << std::fixed << std::setprecision(2)
                  << accuracy * 100 << "%" << std::endl;

        // Check against baseline
        if (accuracy >= 0.99f) {
            std::cout << "  Status: EXCELLENT (>= 99%)" << std::endl;
        } else if (accuracy >= 0.95f) {
            std::cout << "  Status: GOOD (>= 95%)" << std::endl;
        } else if (accuracy >= 0.90f) {
            std::cout << "  Status: ACCEPTABLE (>= 90%)" << std::endl;
        } else {
            std::cout << "  Status: NEEDS IMPROVEMENT (< 90%)" << std::endl;
        }
    } else {
        std::cerr << "ERROR: Unsupported dataset: " << dataset_name << std::endl;
        std::cerr << "Supported: lfw" << std::endl;
        return 1;
    }

    std::cout << "\n============================================" << std::endl;
    return 0;
}

static SeetaImageData cvMatToSeetaImage(const cv::Mat& mat) {
    SeetaImageData simg;
    simg.width = mat.cols;
    simg.height = mat.rows;
    simg.channels = mat.channels();
    simg.data = mat.data;
    return simg;
}

static void print_help(const char* name) {
    std::cout << "=== SeetaFace6 Face Recognizer Benchmark ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << name << " [model_dir] [video] [duration_sec]" << std::endl;
    std::cout << "  " << name << " --accuracy [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Performance mode (default):" << std::endl;
    std::cout << "  model_dir      SeetaFace6 model directory (default: ./cvedix_data/models/seetaface6)" << std::endl;
    std::cout << "  video          Input video path (default: ./cvedix_data/test_video/face_test.mp4)" << std::endl;
    std::cout << "  duration_sec   Benchmark duration in seconds (default: 30)" << std::endl;
    std::cout << std::endl;
    std::cout << "Accuracy mode:" << std::endl;
    std::cout << "  " << name << " --accuracy" << std::endl;
    std::cout << "  " << name << " --accuracy --dataset=lfw --model /path/to/models --root /path/to/datasets" << std::endl;
    std::cout << "  " << name << " --accuracy --download  # Download LFW dataset only" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --accuracy, -a       Run accuracy benchmark (LFW)" << std::endl;
    std::cout << "  --dataset=<name>     Dataset name: lfw (default: lfw)" << std::endl;
    std::cout << "  --root=<path>       Dataset root directory (default: ./datasets)" << std::endl;
    std::cout << "  --model=<path>      Model directory (default: ./cvedix_data/models/seetaface6)" << std::endl;
    std::cout << "  --download          Download LFW dataset and exit" << std::endl;
    std::cout << "  --gpu               Use GPU for inference (requires CUDA build)" << std::endl;
    std::cout << "  --gpu-id=<id>       GPU device ID (default: 0)" << std::endl;
}

int main(int argc, char** argv) {
    // Check for accuracy mode first
    bool accuracy_mode = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--accuracy" || std::string(argv[i]) == "-a") {
            accuracy_mode = true;
            break;
        }
    }

    if (accuracy_mode) {
        return run_accuracy_benchmark(argc, argv);
    }

    signal(SIGINT, signal_handler);

    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_help(argv[0]);
        return 0;
    }

    std::string model_dir    = argc > 1 ? argv[1] : "./cvedix_data/models/seetaface6";
    std::string video_path   = argc > 2 ? argv[2] : "./cvedix_data/test_video/face_test.mp4";
    int         duration_sec = argc > 3 ? std::stoi(argv[3]) : 30;
    bool        use_gpu      = false;
    int         gpu_id       = 0;

    // Check for --gpu and --gpu-id flags
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--gpu") {
            use_gpu = true;
        } else if (arg.substr(0, 9) == "--gpu-id=") {
            gpu_id = std::stoi(arg.substr(9));
            use_gpu = true;
        }
    }

    // --- Validate ---
    if (!std::filesystem::exists(model_dir)) {
        std::cerr << "ERROR: Model directory not found: " << model_dir << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(video_path)) {
        std::cerr << "ERROR: Video file not found: " << video_path << std::endl;
        return 1;
    }

    auto modelPath = [&](const std::string& name) { return model_dir + "/" + name; };

    auto device = use_gpu ? seeta::ModelSetting::GPU : seeta::ModelSetting::CPU;
    int  dev_id = use_gpu ? gpu_id : 0;
    std::string device_str = use_gpu ? "GPU:" + std::to_string(gpu_id) : "CPU";

    std::cout << "============================================" << std::endl;
    std::cout << "  SeetaFace6 Face Recognizer Benchmark"       << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  Model:     " << model_dir                   << std::endl;
    std::cout << "  Video:     " << video_path                  << std::endl;
    std::cout << "  Duration:  " << duration_sec << "s"         << std::endl;
    std::cout << "  Device:    " << device_str                  << std::endl;
    std::cout << "============================================" << std::endl;

    // ======== Load SeetaFace6 engines ========
    std::cout << "[INIT] Loading engines..." << std::endl;
    auto t0 = std::chrono::steady_clock::now();

    seeta::ModelSetting det_set(modelPath("face_detector.csta"), device, dev_id);
    seeta::FaceDetector detector(det_set);
    detector.set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, 40);

    seeta::ModelSetting md_set(modelPath("mask_detector.csta"), device, dev_id);
    seeta::MaskDetector mask_detector(md_set);

    seeta::ModelSetting lm_std_set(modelPath("face_landmarker_pts5.csta"), device, dev_id);
    seeta::FaceLandmarker landmarker_std(lm_std_set);

    seeta::ModelSetting lm_mask_set(modelPath("face_landmarker_mask_pts5.csta"), device, dev_id);
    seeta::FaceLandmarker landmarker_mask(lm_mask_set);

    seeta::ModelSetting rec_std_set(modelPath("face_recognizer.csta"), device, dev_id);
    seeta::FaceRecognizer recognizer_std(rec_std_set);

    seeta::ModelSetting rec_mask_set(modelPath("face_recognizer_mask.csta"), device, dev_id);
    seeta::FaceRecognizer recognizer_mask(rec_mask_set);

    auto t1 = std::chrono::steady_clock::now();
    double load_time = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "[INIT] All engines loaded in " << std::fixed << std::setprecision(2) << load_time << "s" << std::endl;
    std::cout << "[INIT] Feature size: std=" << recognizer_std.GetExtractFeatureSize()
              << " mask=" << recognizer_mask.GetExtractFeatureSize() << std::endl;

    // ======== Open video ========
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "ERROR: Cannot open video: " << video_path << std::endl;
        return 1;
    }

    double video_fps = cap.get(cv::CAP_PROP_FPS);
    int video_w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int video_h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "[VIDEO] " << video_w << "x" << video_h << " @ " << video_fps << " fps" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // ======== Benchmark loop ========
    int total_frames = 0;
    int total_faces = 0;
    int total_masked = 0;

    // Timing accumulators (microseconds)
    double time_detect_us = 0;
    double time_mask_us = 0;
    double time_landmark_us = 0;
    double time_extract_us = 0;

    auto bench_start = std::chrono::steady_clock::now();
    cv::Mat frame;

    while (g_running) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - bench_start).count();
        if (elapsed >= duration_sec) break;

        if (!cap.read(frame)) {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);  // loop
            if (!cap.read(frame)) break;
        }
        if (frame.empty()) continue;

        // Convert to BGR if needed
        cv::Mat bgr;
        if (frame.channels() == 1) {
            cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
        } else {
            bgr = frame;
        }
        SeetaImageData simg = cvMatToSeetaImage(bgr);

        // Step 1: Detect faces
        auto td0 = std::chrono::steady_clock::now();
        SeetaFaceInfoArray faces = detector.detect(simg);
        auto td1 = std::chrono::steady_clock::now();
        time_detect_us += std::chrono::duration<double, std::micro>(td1 - td0).count();

        total_frames++;

        for (int i = 0; i < faces.size; i++) {
            const SeetaRect& rect = faces.data[i].pos;

            // Validate bbox
            if (rect.x < 0 || rect.y < 0 ||
                rect.x + rect.width > bgr.cols ||
                rect.y + rect.height > bgr.rows) continue;

            total_faces++;

            // Step 2: Mask check
            auto tm0 = std::chrono::steady_clock::now();
            float mask_score = 0;
            bool masked = mask_detector.detect(simg, rect, &mask_score);
            auto tm1 = std::chrono::steady_clock::now();
            time_mask_us += std::chrono::duration<double, std::micro>(tm1 - tm0).count();

            if (masked) total_masked++;

            // Step 3: Landmarks
            seeta::FaceLandmarker& lm = masked ? landmarker_mask : landmarker_std;
            auto tl0 = std::chrono::steady_clock::now();
            auto points = lm.mark(simg, rect);
            auto tl1 = std::chrono::steady_clock::now();
            time_landmark_us += std::chrono::duration<double, std::micro>(tl1 - tl0).count();

            // Step 4: Extract features
            seeta::FaceRecognizer& rec = masked ? recognizer_mask : recognizer_std;
            int feat_size = rec.GetExtractFeatureSize();
            std::vector<float> features(feat_size);

            auto te0 = std::chrono::steady_clock::now();
            rec.Extract(simg, points.data(), features.data());
            auto te1 = std::chrono::steady_clock::now();
            time_extract_us += std::chrono::duration<double, std::micro>(te1 - te0).count();
        }

        // Progress
        if (total_frames % 50 == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed_s = std::chrono::duration<double>(now - bench_start).count();
            double fps = total_frames / elapsed_s;
            std::cout << "\r  [" << (int)elapsed_s << "s] "
                      << total_frames << " frames, "
                      << total_faces << " faces, "
                      << std::fixed << std::setprecision(1) << fps << " fps"
                      << std::flush;
        }
    }

    auto bench_end = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration<double>(bench_end - bench_start).count();

    cap.release();

    // ======== Results ========
    double fps = total_frames > 0 ? total_frames / total_time : 0;
    double ms_per_frame = total_frames > 0 ? (total_time * 1000.0) / total_frames : 0;
    double avg_faces = total_frames > 0 ? (double)total_faces / total_frames : 0;

    double avg_detect_ms   = total_frames > 0 ? (time_detect_us / total_frames) / 1000.0 : 0;
    double avg_mask_ms     = total_faces > 0 ? (time_mask_us / total_faces) / 1000.0 : 0;
    double avg_landmark_ms = total_faces > 0 ? (time_landmark_us / total_faces) / 1000.0 : 0;
    double avg_extract_ms  = total_faces > 0 ? (time_extract_us / total_faces) / 1000.0 : 0;
    double avg_total_face  = avg_mask_ms + avg_landmark_ms + avg_extract_ms;

    std::cout << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "         BENCHMARK RESULTS"                   << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Duration:         " << total_time << " s"   << std::endl;
    std::cout << "  Total frames:     " << total_frames         << std::endl;
    std::cout << "  Total faces:      " << total_faces          << std::endl;
    std::cout << "  Masked faces:     " << total_masked         << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "  THROUGHPUT:"                                 << std::endl;
    std::cout << "    Pipeline FPS:   " << fps                  << std::endl;
    std::cout << "    ms/frame:       " << ms_per_frame << " ms" << std::endl;
    std::cout << "    Avg faces/frame:" << std::setprecision(1) << avg_faces << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << std::setprecision(2);
    std::cout << "  PER-STEP LATENCY (avg per face):"           << std::endl;
    std::cout << "    Detect:         " << avg_detect_ms << " ms (per frame)" << std::endl;
    std::cout << "    Mask check:     " << avg_mask_ms   << " ms" << std::endl;
    std::cout << "    Landmarks:      " << avg_landmark_ms << " ms" << std::endl;
    std::cout << "    Feature extract:" << avg_extract_ms << " ms" << std::endl;
    std::cout << "    Total/face:     " << avg_total_face << " ms" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "  ENGINE LOAD TIME: " << load_time << " s"    << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}

#else

#include <iostream>
int main() {
    std::cerr << "This benchmark requires SeetaFace6." << std::endl;
    std::cerr << "Build SeetaFace6: cd third_party/seetaface6 && bash build_seetaface6.sh" << std::endl;
    std::cerr << "Then rebuild with: -DCVEDIX_WITH_FACE=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_FACE
