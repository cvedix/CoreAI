#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_mllm_analyser_node.h"
#include "cvedix/nodes/osd/cvedix_mllm_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## mllm_analyse_sample ##
* image(frame) analyse based on Multimodal Large Language Model(from Ollama).
* read images from disk and analyse the image using MLLM using the prepared prompt.
*/
int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_file_src_0", 0, "./cvedix_data/test_images/llm/writing/%d.jpg", 2, 0.5);
    auto understanding_prompt = "一句话描述图片内容，要求包含：\n"
                                "1. 对天气的描述\n"
                                "2. 对环境的描述\n"
                                "3. 对位置的描述（如果可以从图片上的文字信息得出）\n"
                                "4. 字数不超过50字"
                                "你的输出结果是：";
    auto writing_prompt = "根据图片写一段故事，要求包含：\n"
                                "1. 完整的故事结构\n"
                                "2. 故事内容要包含时间、地点、人物等元素"
                                "3. 字数不超过50字"
                                "你的输出结果是：";
    auto mllm_analyser_0 = std::make_shared<cvedix_nodes::cvedix_mllm_analyser_node>("mllm_analyser_0",                      // node name
                                                                             "minicpm-v:8b",                         // mllm model name (support image as input)
                                                                             writing_prompt,                         // prompt
                                                                             "http://192.168.77.219:11434",          // api base url
                                                                             "",                                     // api key (not required by Ollama)
                                                                             llmlib::LLMBackendType::Ollama);        // backend type, make sure Ollama is installed at 192.168.77.219
    auto mllm_osd_0 = std::make_shared<cvedix_nodes::cvedix_mllm_osd_node>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    mllm_analyser_0->attach_to({image_src_0});
    mllm_osd_0->attach_to({mllm_analyser_0});
    screen_des_0->attach_to({mllm_osd_0});

    image_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({image_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    image_src_0->detach_recursively();
}