/**
 * @file test_cvedix_node.cpp
 * @brief Unit tests for cvedix_node's queue and threading contract.
 *
 * These cover the guarantees the whole pipeline rests on:
 *   - every meta pushed into a node reaches its downstream node, in order
 *   - a handler that throws is contained: the node keeps running and the process
 *     survives (a bare `throw` used to escape handle_run() and std::terminate())
 *   - a full in_queue drops instead of blocking the producer
 *   - concurrent producers do not lose or corrupt metas (in_queue/out_queue are
 *     only ever touched under their mutex)
 *   - shutdown joins both internal threads and is safe to repeat
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "cvedix/objects/cvedix_control_meta.h"

#include "test_assert.h"
#include "test_nodes.h"

using cvedix_test::counting_des_node;
using cvedix_test::counting_mid_node;
using cvedix_test::make_frame;
using cvedix_test::throwing_mid_node;
using cvedix_test::wait_for;

namespace {

constexpr int kFrameCount = 50;

}  // namespace

// --------------------------------------------------------------------------
// Delivery
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(mid_forwards_every_meta_to_des_in_order) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    for (int i = 0; i < kFrameCount; ++i) {
        mid->meta_flow(make_frame(i));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == static_cast<size_t>(kFrameCount); }));
    CVEDIX_ASSERT_EQ(mid->handled_count(), static_cast<size_t>(kFrameCount));

    const auto indices = des->received_indices();
    CVEDIX_ASSERT_EQ(indices.size(), static_cast<size_t>(kFrameCount));
    for (int i = 0; i < kFrameCount; ++i) {
        CVEDIX_ASSERT_EQ(indices[static_cast<size_t>(i)], i);
    }

    des->stop();
    mid->stop();
}

CVEDIX_TEST_CASE(des_returning_nullptr_stops_propagation) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(make_frame(0));
    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == 1; }));

    // A DES node must never publish onward, so it has no next nodes at all.
    CVEDIX_ASSERT_TRUE(des->next_nodes().empty());

    des->stop();
    mid->stop();
}

CVEDIX_TEST_CASE(control_meta_reaches_downstream_handler) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(std::make_shared<cvedix_objects::cvedix_control_meta>(
        cvedix_objects::cvedix_control_type::VIDEO_RECORD, 0));

    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->control_count() == 1; }));
    CVEDIX_ASSERT_EQ(des->received_count(), static_cast<size_t>(0));

    des->stop();
    mid->stop();
}

CVEDIX_TEST_CASE(null_meta_is_ignored_by_meta_flow) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(nullptr);
    mid->meta_flow(make_frame(7));

    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == 1; }));
    CVEDIX_ASSERT_EQ(des->received_indices().front(), 7);

    des->stop();
    mid->stop();
}

// --------------------------------------------------------------------------
// Exception containment (regression test: a throwing handler used to abort)
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(throwing_handler_does_not_kill_the_node) {
    auto thrower = std::make_shared<throwing_mid_node>("thrower", /*throw_on_even=*/true);
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({thrower});

    constexpr int total = 20;
    for (int i = 0; i < total; ++i) {
        thrower->meta_flow(make_frame(i));
    }

    // Every meta must be attempted: the loop has to keep draining in_queue even
    // after a handler throws, otherwise one bad frame stalls the node forever.
    CVEDIX_ASSERT_TRUE(wait_for([&] { return thrower->attempted_count() == static_cast<size_t>(total); }));
    CVEDIX_ASSERT_EQ(thrower->thrown_count(), static_cast<size_t>(total / 2));

    // Only the non-throwing (odd) frames are forwarded; thrown ones are dropped.
    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == static_cast<size_t>(total / 2); }));
    for (int index : des->received_indices()) {
        CVEDIX_ASSERT_TRUE(index % 2 == 1);
    }

    des->stop();
    thrower->stop();
}

CVEDIX_TEST_CASE(node_still_usable_after_a_handler_threw) {
    auto thrower = std::make_shared<throwing_mid_node>("thrower", /*throw_on_even=*/true);
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({thrower});

    thrower->meta_flow(make_frame(0));   // throws
    CVEDIX_ASSERT_TRUE(wait_for([&] { return thrower->thrown_count() == 1; }));

    thrower->meta_flow(make_frame(1));   // must still be processed
    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == 1; }));
    CVEDIX_ASSERT_EQ(des->received_indices().front(), 1);

    des->stop();
    thrower->stop();
}

// --------------------------------------------------------------------------
// Backpressure
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(full_in_queue_drops_without_blocking_producer) {
    // A slow handler plus a 2-deep queue means the producer must be dropping,
    // not waiting: meta_flow() is called from the upstream dispatch thread and
    // blocking there would stall the entire pipeline.
    auto mid = std::make_shared<counting_mid_node>("slow_mid", std::chrono::milliseconds(30));
    mid->set_max_in_queue_size(2);

    constexpr int total = 100;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < total; ++i) {
        mid->meta_flow(make_frame(i));
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    // Serial processing would need total * 30ms == 3000ms. Pushing must be O(1).
    CVEDIX_ASSERT_LE(elapsed.count(), 1000);
    CVEDIX_ASSERT_TRUE(mid->handled_count() < static_cast<size_t>(total));

    mid->stop();
}

CVEDIX_TEST_CASE(no_drops_when_queue_is_large_enough) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    mid->set_max_in_queue_size(kFrameCount * 4);
    des->set_max_in_queue_size(kFrameCount * 4);
    des->attach_to({mid});

    for (int i = 0; i < kFrameCount; ++i) {
        mid->meta_flow(make_frame(i));
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == static_cast<size_t>(kFrameCount); }));

    des->stop();
    mid->stop();
}

// --------------------------------------------------------------------------
// Concurrency
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(concurrent_producers_lose_no_metas) {
    // in_queue/out_queue are shared between the producers, the handle thread and
    // the dispatch thread. If any of them touched size()/front()/push() outside
    // the mutex this test would lose metas or crash under a sanitizer.
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");

    constexpr int producers = 4;
    constexpr int per_producer = 100;
    constexpr int total = producers * per_producer;

    mid->set_max_in_queue_size(total * 2);
    des->set_max_in_queue_size(total * 2);
    des->attach_to({mid});

    std::vector<std::thread> threads;
    threads.reserve(producers);
    for (int p = 0; p < producers; ++p) {
        threads.emplace_back([mid, p] {
            for (int i = 0; i < per_producer; ++i) {
                mid->meta_flow(make_frame(p * per_producer + i));
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == static_cast<size_t>(total); },
                                std::chrono::milliseconds(15000)));
    CVEDIX_ASSERT_EQ(mid->handled_count(), static_cast<size_t>(total));

    des->stop();
    mid->stop();
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(stop_joins_threads_and_is_idempotent) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(make_frame(0));
    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == 1; }));

    des->stop();
    mid->stop();
    // Repeating shutdown must not double-join or hang.
    mid->stop();
    des->stop();

    // Pushing into a stopped node is a no-op, not a crash.
    mid->meta_flow(make_frame(1));
    CVEDIX_ASSERT_EQ(des->received_count(), static_cast<size_t>(1));
}

CVEDIX_TEST_CASE(detach_stops_further_delivery) {
    auto mid = std::make_shared<counting_mid_node>("mid");
    auto des = std::make_shared<counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(make_frame(0));
    CVEDIX_ASSERT_TRUE(wait_for([&] { return des->received_count() == 1; }));

    des->detach();
    CVEDIX_ASSERT_TRUE(mid->next_nodes().empty());

    for (int i = 1; i <= 5; ++i) {
        mid->meta_flow(make_frame(i));
    }
    CVEDIX_ASSERT_TRUE(wait_for([&] { return mid->handled_count() == 6; }));
    CVEDIX_ASSERT_EQ(des->received_count(), static_cast<size_t>(1));

    des->stop();
    mid->stop();
}

CVEDIX_TEST_CASE(attaching_downstream_of_a_des_node_throws) {
    // attach_to() enforces the DAG rules: a DES node must never gain next nodes,
    // and the violation has to throw rather than silently build a broken graph.
    auto des = std::make_shared<counting_des_node>("des");
    auto tail = std::make_shared<counting_mid_node>("tail");

    bool threw = false;
    try {
        tail->attach_to({des});
    }
    catch (const std::exception&) {
        threw = true;
    }
    CVEDIX_ASSERT_TRUE(threw);
    CVEDIX_ASSERT_TRUE(des->next_nodes().empty());

    tail->stop();
    des->stop();
}

int main(int argc, char** argv) {
    // Tests push thousands of metas and every meta_flow() emits DEBUG lines;
    // logging at the default level would dominate the runtime and fill ./log.
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
    CVEDIX_SET_LOG_TO_FILE(false);
    CVEDIX_LOGGER_INIT();
    return cvedix_test::run_all(argc, argv);
}
