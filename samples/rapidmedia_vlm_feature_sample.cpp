#ifdef CVEDIX_WITH_LLM

#include "cvedix/nodes/broker/cvedix_console_broker_node.h"
#include "cvedix/nodes/infers/cvedix_rapidmedia_vlm_feature_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"

#include "sample_options.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

/*
 * RapidMedia VLM Feature Sample
 *
 * Pipeline:
 *   file_src -> yolo_detector -> sort_track -> rapidmedia_vlm_feature -> console_broker
 *
 * Usage:
 *   ./rapidmedia_vlm_feature_sample \
 *     --video /path/to/video.mp4 \
 *     --model qwen3-vl:2b \
 *     --api http://127.0.0.1:8080 \
 *     --backend openai \
 *     [--detector-model /path/to/yolo12n.engine] \
 *     [--labels /path/to/coco.txt]
 *
 * Paths also read from CVEDIX_VIDEO / CVEDIX_MODEL / CVEDIX_LABELS, and relative
 * defaults resolve under CVEDIX_DATA_DIR (default: ./cvedix_data).
 */

namespace {

llmlib::LLMBackendType parse_backend(const std::string& raw) {
    std::string value = raw;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "ollama") {
        return llmlib::LLMBackendType::Ollama;
    }

    // llama.cpp/coreai expose OpenAI-compatible API in this integration flow.
    return llmlib::LLMBackendType::OpenAI;
}

}  // namespace

int main(int argc, char** argv) {
    std::string video_path = sample_options::resolve(
        argc, argv, "--video", "CVEDIX_VIDEO", sample_options::data_path("video/sample.mp4"));
    std::string detector_model = sample_options::resolve(
        argc, argv, "--detector-model", "CVEDIX_MODEL", sample_options::data_path("yolo12n.engine"));
    std::string labels_path = sample_options::resolve(
        argc, argv, "--labels", "CVEDIX_LABELS", sample_options::data_path("coco.txt"));
    std::string model_name = "qwen3-vl:2b";
    std::string api_base_url = sample_options::env_or("CVEDIX_VLM_API", "http://127.0.0.1:8080");
    std::string api_key = sample_options::env_or("CVEDIX_VLM_API_KEY");
    std::string backend_name = "openai";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc) {
            video_path = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_name = argv[++i];
        } else if (arg == "--api" && i + 1 < argc) {
            api_base_url = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            api_key = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            backend_name = argv[++i];
        } else if (arg == "--detector-model" && i + 1 < argc) {
            detector_model = argv[++i];
        } else if (arg == "--labels" && i + 1 < argc) {
            labels_path = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            const std::string level = argv[++i];
            if (level == "debug" || level == "verbose") {
                CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);
            } else if (level == "info") {
                CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
            } else if (level == "warning" || level == "warn") {
                CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
            } else if (level == "error") {
                CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
            }
        }
    }

    if (!sample_options::check_exists("video", video_path) ||
        !sample_options::check_exists("detector-model", detector_model)) {
        return 1;
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    cv::setNumThreads(4);

    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0",
        0,
        video_path,
        1.0,
        true,
        "",
        3,
        true);

    auto yolo_detector_0 = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector_0",
        detector_model,
        cvedix_nodes::YoloVersion::YOLO12,
        labels_path,
        0.30f,
        0.5f,
        0,
        cvedix_nodes::BackendType::TENSORRT);

    auto sort_track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_track_0",
        cvedix_nodes::cvedix_track_for::NORMAL);

    auto rapidmedia_vlm_feature_0 = std::make_shared<cvedix_nodes::cvedix_rapidmedia_vlm_feature_node>(
        "rapidmedia_vlm_feature_0",
        model_name,
        api_base_url,
        api_key,
        parse_backend(backend_name),
        4,
        4,
        0.02f,
        0.02f,
        12);

    auto console_broker_0 = std::make_shared<cvedix_nodes::cvedix_console_broker_node>(
        "console_broker_0",
        cvedix_nodes::cvedix_broke_for::NORMAL,
        20,
        100);

    yolo_detector_0->attach_to({file_src_0});
    sort_track_0->attach_to({yolo_detector_0});
    rapidmedia_vlm_feature_0->attach_to({sort_track_0});
    console_broker_0->attach_to({rapidmedia_vlm_feature_0});

    yolo_detector_0->set_max_in_queue_size(1);
    sort_track_0->set_max_in_queue_size(1);
    rapidmedia_vlm_feature_0->set_max_in_queue_size(1);
    console_broker_0->set_max_in_queue_size(1);

    file_src_0->start();

    std::cout
        << "\nRapidMedia VLM feature pipeline is running.\n"
        << "- Video: " << video_path << "\n"
        << "- Model: " << model_name << "\n"
        << "- API: " << api_base_url << "\n"
        << "- Backend: " << backend_name << "\n"
        << "Inspect console output: targets[*].embeddings + secondary_labels(vlm:...)\n"
        << "Press Enter to stop...\n\n";

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires CVEDIX_WITH_LLM=ON" << std::endl;
    return 1;
}

#endif
