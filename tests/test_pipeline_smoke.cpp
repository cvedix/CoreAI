/**
 * @file test_pipeline_smoke.cpp
 * @brief End-to-end smoke test for pipeline construction and metadata flow.
 *
 * Exercises the wiring layer — attach_to(), the publisher/subscriber list, and
 * each node's dispatch thread — with instrumented nodes only. No video files,
 * no models and no GPU, so this runs anywhere the library builds and is a
 * reasonable CI gate for "did we break the dataflow?".
 */

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "test_assert.h"
#include "test_nodes.h"

using cvedix_test::counting_des_node;
using cvedix_test::counting_mid_node;
using cvedix_test::make_frame;
using cvedix_test::tagging_mid_node;
using cvedix_test::wait_for;

namespace {

constexpr int kFrameCount = 25;

}  // namespace

CVEDIX_TEST_CASE(four_stage_chain_runs_every_stage_in_order) {
    // head -> A -> B -> sink. Each tagging stage appends to description, so the
    // final string proves both that every stage ran and that it ran in order.
    auto head = std::make_shared<counting_mid_node>("head");
    auto stage_a = std::make_shared<tagging_mid_node>("stage_a", "|A");
    auto stage_b = std::make_shared<tagging_mid_node>("stage_b", "|B");
    auto sink = std::make_shared<counting_des_node>("sink");

    stage_a->attach_to({head});
    stage_b->attach_to({stage_a});
    sink->attach_to({stage_b});

    for (int i = 0; i < kFrameCount; ++i) {
        head->meta_flow(make_frame(i));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink->received_count() == static_cast<size_t>(kFrameCount); }));

    for (const auto& description : sink->received_descriptions()) {
        CVEDIX_ASSERT_EQ(description, std::string("|A|B"));
    }

    const auto indices = sink->received_indices();
    for (int i = 0; i < kFrameCount; ++i) {
        CVEDIX_ASSERT_EQ(indices[static_cast<size_t>(i)], i);
    }

    sink->detach_recursively();
    sink->stop();
    stage_b->stop();
    stage_a->stop();
    head->stop();
}

CVEDIX_TEST_CASE(fan_out_delivers_to_every_branch) {
    // push_meta() hands the same shared_ptr to every subscriber, so both sinks
    // must see the full stream.
    auto head = std::make_shared<counting_mid_node>("head");
    auto sink_a = std::make_shared<counting_des_node>("sink_a");
    auto sink_b = std::make_shared<counting_des_node>("sink_b");

    sink_a->attach_to({head});
    sink_b->attach_to({head});

    for (int i = 0; i < kFrameCount; ++i) {
        head->meta_flow(make_frame(i));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] {
        return sink_a->received_count() == static_cast<size_t>(kFrameCount) &&
               sink_b->received_count() == static_cast<size_t>(kFrameCount);
    }));
    CVEDIX_ASSERT_EQ(head->next_nodes().size(), static_cast<size_t>(2));

    sink_a->stop();
    sink_b->stop();
    head->stop();
}

CVEDIX_TEST_CASE(fan_in_merges_both_branches) {
    // Two independent heads feeding one sink: the sink's in_queue is written by
    // two dispatch threads at once, which is the common multi-channel shape.
    auto head_a = std::make_shared<counting_mid_node>("head_a");
    auto head_b = std::make_shared<counting_mid_node>("head_b");
    auto sink = std::make_shared<counting_des_node>("sink");
    sink->set_max_in_queue_size(kFrameCount * 4);

    sink->attach_to({head_a, head_b});

    for (int i = 0; i < kFrameCount; ++i) {
        head_a->meta_flow(make_frame(i, /*channel_index=*/0));
        head_b->meta_flow(make_frame(i, /*channel_index=*/1));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink->received_count() == static_cast<size_t>(kFrameCount * 2); }));

    sink->stop();
    head_b->stop();
    head_a->stop();
}

CVEDIX_TEST_CASE(detaching_one_branch_leaves_the_other_running) {
    auto head = std::make_shared<counting_mid_node>("head");
    auto sink_a = std::make_shared<counting_des_node>("sink_a");
    auto sink_b = std::make_shared<counting_des_node>("sink_b");

    sink_a->attach_to({head});
    sink_b->attach_to({head});

    head->meta_flow(make_frame(0));
    CVEDIX_ASSERT_TRUE(wait_for([&] {
        return sink_a->received_count() == 1 && sink_b->received_count() == 1;
    }));

    sink_a->detach();
    CVEDIX_ASSERT_EQ(head->next_nodes().size(), static_cast<size_t>(1));

    for (int i = 1; i <= 5; ++i) {
        head->meta_flow(make_frame(i));
    }
    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink_b->received_count() == 6; }));
    CVEDIX_ASSERT_EQ(sink_a->received_count(), static_cast<size_t>(1));

    sink_a->stop();
    sink_b->stop();
    head->stop();
}

CVEDIX_TEST_CASE(detach_recursively_tears_down_the_whole_chain) {
    auto head = std::make_shared<counting_mid_node>("head");
    auto stage = std::make_shared<tagging_mid_node>("stage", "|A");
    auto sink = std::make_shared<counting_des_node>("sink");

    stage->attach_to({head});
    sink->attach_to({stage});

    head->meta_flow(make_frame(0));
    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink->received_count() == 1; }));

    head->detach_recursively();

    CVEDIX_ASSERT_TRUE(head->next_nodes().empty());
    CVEDIX_ASSERT_TRUE(stage->next_nodes().empty());

    for (int i = 1; i <= 5; ++i) {
        head->meta_flow(make_frame(i));
    }
    CVEDIX_ASSERT_TRUE(wait_for([&] { return head->handled_count() == 6; }));
    CVEDIX_ASSERT_EQ(sink->received_count(), static_cast<size_t>(1));

    sink->stop();
    stage->stop();
    head->stop();
}

CVEDIX_TEST_CASE(a_slow_stage_does_not_lose_metas_upstream) {
    // The whole point of the per-node queues: a slow middle stage must buffer
    // rather than drop, as long as its queue is deep enough.
    auto head = std::make_shared<counting_mid_node>("head");
    auto slow = std::make_shared<counting_mid_node>("slow", std::chrono::milliseconds(5));
    auto sink = std::make_shared<counting_des_node>("sink");

    slow->set_max_in_queue_size(kFrameCount * 4);
    sink->set_max_in_queue_size(kFrameCount * 4);

    slow->attach_to({head});
    sink->attach_to({slow});

    for (int i = 0; i < kFrameCount; ++i) {
        head->meta_flow(make_frame(i));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink->received_count() == static_cast<size_t>(kFrameCount); },
                                std::chrono::milliseconds(10000)));

    sink->stop();
    slow->stop();
    head->stop();
}

CVEDIX_TEST_CASE(teardown_in_upstream_order_does_not_hang) {
    // Stopping the head first means downstream nodes are still running while
    // their producer disappears; joining must still complete.
    auto head = std::make_shared<counting_mid_node>("head");
    auto stage = std::make_shared<tagging_mid_node>("stage", "|A");
    auto sink = std::make_shared<counting_des_node>("sink");

    stage->attach_to({head});
    sink->attach_to({stage});

    for (int i = 0; i < kFrameCount; ++i) {
        head->meta_flow(make_frame(i));
    }
    CVEDIX_ASSERT_TRUE(wait_for([&] { return sink->received_count() == static_cast<size_t>(kFrameCount); }));

    head->stop();
    stage->stop();
    sink->stop();
}

int main(int argc, char** argv) {
    // Tests push thousands of metas and every meta_flow() emits DEBUG lines;
    // logging at the default level would dominate the runtime and fill ./log.
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
    CVEDIX_SET_LOG_TO_FILE(false);
    CVEDIX_LOGGER_INIT();
    return cvedix_test::run_all(argc, argv);
}
