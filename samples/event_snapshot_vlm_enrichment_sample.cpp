#ifdef CVEDIX_WITH_LLM

#include "cvedix/nodes/infers/cvedix_vlm_feature_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/objects/cvedix_frame_target.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void exit_process(int code, std::ofstream* ofs = nullptr) {
    if (ofs && ofs->is_open()) {
        ofs->flush();
    }
    std::cout.flush();
    std::cerr.flush();
    std::_Exit(code);
}

class event_snapshot_vlm_runner : public cvedix_nodes::cvedix_vlm_feature_node {
public:
    using cvedix_nodes::cvedix_vlm_feature_node::cvedix_vlm_feature_node;

    std::shared_ptr<cvedix_objects::cvedix_frame_meta> run(
        const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta) {
        auto out = handle_frame_meta(meta);
        return std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(out);
    }
};

bool has_image_ext(const std::filesystem::path& p) {
    if (!p.has_extension()) {
        return false;
    }

    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
           ext == ".bmp" || ext == ".webp";
}

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (const char ch : in) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::vector<std::filesystem::path> collect_images(const std::string& input_path) {
    std::vector<std::filesystem::path> images;
    const std::filesystem::path p(input_path);

    if (!std::filesystem::exists(p)) {
        return images;
    }

    if (std::filesystem::is_regular_file(p)) {
        if (has_image_ext(p)) {
            images.push_back(std::filesystem::absolute(p));
        }
        return images;
    }

    for (const auto& entry : std::filesystem::directory_iterator(p)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto file_path = entry.path();
        if (has_image_ext(file_path)) {
            images.push_back(std::filesystem::absolute(file_path));
        }
    }

    std::sort(images.begin(), images.end());
    return images;
}

llmlib::LLMBackendType parse_backend(const std::string& raw) {
    std::string value = raw;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "openai") {
        return llmlib::LLMBackendType::OpenAI;
    }
    return llmlib::LLMBackendType::Ollama;
}

std::string first_vlm_label(const std::shared_ptr<cvedix_objects::cvedix_frame_target>& target) {
    if (!target) {
        return "";
    }
    for (const auto& label : target->secondary_labels) {
        if (label.rfind("vlm:", 0) == 0) {
            return label;
        }
    }
    return "";
}

}  // namespace

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    std::string input_path = "/home/cvedix/rapidmedia/ass-admin/data/ai-events";
    std::string model_name = "qwen3-vl:latest";
    std::string api_base_url = "http://127.0.0.1:11434";
    std::string api_key = "";
    std::string backend_name = "ollama";
    std::string output_jsonl = "";
    int max_images = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_name = argv[++i];
        } else if (arg == "--api" && i + 1 < argc) {
            api_base_url = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            api_key = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            backend_name = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_jsonl = argv[++i];
        } else if (arg == "--max" && i + 1 < argc) {
            max_images = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--help") {
            std::cout
                << "Usage: event_snapshot_vlm_enrichment_sample [options]\n"
                << "  --input <file_or_dir>     Event snapshot image file or directory\n"
                << "  --model <name>            VLM model name\n"
                << "  --api <url>               LLM API base URL\n"
                << "  --api-key <key>           API key for OpenAI-compatible backend\n"
                << "  --backend <ollama|openai> LLM backend type\n"
                << "  --output <path>           Output JSONL file\n"
                << "  --max <n>                 Max images to process (0 = all)\n";
            return 0;
        }
    }

    std::ofstream ofs;

    const auto images = collect_images(input_path);
    if (images.empty()) {
        std::cerr << "No image found from input: " << input_path << std::endl;
        exit_process(1, &ofs);
    }

    if (!output_jsonl.empty()) {
        ofs.open(output_jsonl, std::ios::out | std::ios::trunc);
        if (!ofs) {
            std::cerr << "Cannot open output file: " << output_jsonl << std::endl;
            exit_process(1, &ofs);
        }
    }

    try {
        auto runner = std::make_shared<event_snapshot_vlm_runner>(
            "event_snapshot_vlm_runner",
            model_name,
            api_base_url,
            api_key,
            parse_backend(backend_name),
            1,
            1,
            0.0f,
            0.0f,
            12);

        int processed = 0;
        int failed = 0;
        for (size_t i = 0; i < images.size(); ++i) {
            if (max_images > 0 && processed >= max_images) {
                break;
            }

            const std::string image_path = images[i].string();
            cv::Mat frame = cv::imread(image_path, cv::IMREAD_COLOR);
            if (frame.empty()) {
                ++failed;
                continue;
            }

            const int frame_index = static_cast<int>(i);
            auto meta = std::make_shared<cvedix_objects::cvedix_frame_meta>(
                frame,
                frame_index,
                0,
                frame.cols,
                frame.rows,
                0);

            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                0,
                0,
                frame.cols,
                frame.rows,
                0,
                1.0f,
                frame_index,
                0,
                "event_object");
            meta->targets.push_back(target);

            auto out_meta = runner->run(meta);
            if (!out_meta || out_meta->targets.empty() || !out_meta->targets[0]) {
                ++failed;
                continue;
            }

            const auto& out_target = out_meta->targets[0];
            const std::string vlm_label = first_vlm_label(out_target);
            const int embedding_dim = static_cast<int>(out_target->embeddings.size());

            std::ostringstream line;
            line << "{"
                 << "\"image\":\"" << json_escape(image_path) << "\","
                 << "\"embedding_dim\":" << embedding_dim << ","
                 << "\"vlm_label\":\"" << json_escape(vlm_label) << "\""
                 << "}";

            std::cout << line.str() << std::endl;
            if (ofs) {
                ofs << line.str() << "\n";
            }

            ++processed;
        }

        std::cout
            << "Processed=" << processed
            << ", Failed=" << failed
            << ", InputTotal=" << images.size()
            << std::endl;

        exit_process(processed > 0 ? 0 : 2, &ofs);
    } catch (const char* err) {
        std::cerr << "VLM sample fatal error: " << (err ? err : "unknown") << std::endl;
        exit_process(3, &ofs);
    } catch (const std::exception& ex) {
        std::cerr << "VLM sample fatal error: " << ex.what() << std::endl;
        exit_process(3, &ofs);
    } catch (...) {
        std::cerr << "VLM sample fatal error: unknown" << std::endl;
        exit_process(3, &ofs);
    }
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires CVEDIX_WITH_LLM=ON" << std::endl;
    return 1;
}

#endif
