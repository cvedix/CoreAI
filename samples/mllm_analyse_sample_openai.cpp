#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_mllm_analyser_node.h"
#include "cvedix/nodes/osd/cvedix_mllm_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## mllm_analyse_sample_openai ##
* image(frame) analyse based on Multimodal Large Language Model(from aliyun or other OpenAI-compatible api services).
* read images from disk and analyse the image using MLLM using the prepared prompt.
*/
int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_file_src_0", 0, "./cvedix_data/test_images/llm/understanding/%d.jpg", 2, 0.5);
    auto writing_prompt = "给图片打标签，要求包含：\n"
                          "1. 先仔细观察图片内容，为图片赋予适合的标签\n"
                          "2. 给出的标签最多不超过5个\n"
                          "3. 输出按以下格式：\n"
                          "通过仔细观察图片，可以为图片赋予这些标签：['标签1', '标签2', '标签3']。";
    auto mllm_analyser_0 = std::make_shared<cvedix_nodes::cvedix_mllm_analyser_node>("mllm_analyser_0",                                   // node name
                                                                             "qwen-vl-max",                                       // mllm model name (from aliyun, support image as input)
                                                                             writing_prompt,                                      // prompt
                                                                             "https://dashscope.aliyuncs.com/compatible-mode/v1", // api base url
                                                                             "sk-XXX",                                            // api key (from aliyun)
                                                                             llmlib::LLMBackendType::OpenAI);                     // backend type
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