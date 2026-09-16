#pragma once

/**
 * @file test_nodes.h
 * @brief Instrumented cvedix_node subclasses used by the tests.
 *
 * cvedix_node's constructor, handle_frame_meta() and initialized()/deinitialized()
 * are all protected, so exercising the base class requires purpose-built
 * subclasses. These are deliberately behaviour-free apart from counting, so a
 * failing test points at cvedix_node rather than at a real node's logic.
 */

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_control_meta.h"
#include "cvedix/objects/cvedix_frame_meta.h"

namespace cvedix_test {

/**
 * @brief MID node that records every frame index it handles and forwards it.
 *
 * @param delay Artificial per-frame processing cost, used to build up queue
 *              depth in the backpressure tests.
 */
class counting_mid_node : public cvedix_nodes::cvedix_node {
public:
    explicit counting_mid_node(const std::string& name,
                              std::chrono::milliseconds delay = std::chrono::milliseconds(0))
        : cvedix_nodes::cvedix_node(name), delay_(delay) {
        initialized();
    }

    ~counting_mid_node() override { stop(); }

    /// @brief Join the internal threads; safe to call more than once.
    void stop() {
        if (!stopped_.exchange(true)) {
            deinitialized();
        }
    }

    cvedix_nodes::cvedix_node_type node_type() override { return cvedix_nodes::cvedix_node_type::MID; }

    size_t handled_count() const { return handled_.load(); }

    std::vector<int> handled_indices() const {
        std::lock_guard<std::mutex> guard(indices_lock_);
        return indices_;
    }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        if (delay_.count() > 0) {
            std::this_thread::sleep_for(delay_);
        }
        {
            std::lock_guard<std::mutex> guard(indices_lock_);
            indices_.push_back(meta->frame_index);
        }
        handled_.fetch_add(1);
        return meta;
    }

private:
    std::chrono::milliseconds delay_;
    std::atomic<size_t> handled_{0};
    std::atomic<bool> stopped_{false};
    mutable std::mutex indices_lock_;
    std::vector<int> indices_;
};

/**
 * @brief DES node: records what arrives and returns nullptr so nothing is forwarded.
 */
class counting_des_node : public cvedix_nodes::cvedix_node {
public:
    explicit counting_des_node(const std::string& name) : cvedix_nodes::cvedix_node(name) {
        initialized();
    }

    ~counting_des_node() override { stop(); }

    void stop() {
        if (!stopped_.exchange(true)) {
            deinitialized();
        }
    }

    cvedix_nodes::cvedix_node_type node_type() override { return cvedix_nodes::cvedix_node_type::DES; }

    size_t received_count() const { return received_.load(); }

    std::vector<int> received_indices() const {
        std::lock_guard<std::mutex> guard(indices_lock_);
        return indices_;
    }

    /// @brief Descriptions accumulated upstream, used by the pipeline smoke test.
    std::vector<std::string> received_descriptions() const {
        std::lock_guard<std::mutex> guard(indices_lock_);
        return descriptions_;
    }

    size_t control_count() const { return control_.load(); }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        {
            std::lock_guard<std::mutex> guard(indices_lock_);
            indices_.push_back(meta->frame_index);
            descriptions_.push_back(meta->description);
        }
        received_.fetch_add(1);
        return nullptr;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override {
        (void)meta;
        control_.fetch_add(1);
        return nullptr;
    }

private:
    std::atomic<size_t> received_{0};
    std::atomic<size_t> control_{0};
    std::atomic<bool> stopped_{false};
    mutable std::mutex indices_lock_;
    std::vector<int> indices_;
    std::vector<std::string> descriptions_;
};

/**
 * @brief MID node whose handler throws for selected frames.
 *
 * Used to prove that a throwing handler is contained inside handle_run() instead
 * of escaping the thread and calling std::terminate().
 */
class throwing_mid_node : public cvedix_nodes::cvedix_node {
public:
    /// @param throw_on_even throw for even frame_index, otherwise forward normally
    explicit throwing_mid_node(const std::string& name, bool throw_on_even = true)
        : cvedix_nodes::cvedix_node(name), throw_on_even_(throw_on_even) {
        initialized();
    }

    ~throwing_mid_node() override { stop(); }

    void stop() {
        if (!stopped_.exchange(true)) {
            deinitialized();
        }
    }

    cvedix_nodes::cvedix_node_type node_type() override { return cvedix_nodes::cvedix_node_type::MID; }

    size_t attempted_count() const { return attempted_.load(); }
    size_t thrown_count() const { return thrown_.load(); }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        attempted_.fetch_add(1);
        const bool is_even = (meta->frame_index % 2) == 0;
        if (throw_on_even_ == is_even) {
            thrown_.fetch_add(1);
            throw std::runtime_error("intentional handler failure");
        }
        return meta;
    }

private:
    bool throw_on_even_;
    std::atomic<size_t> attempted_{0};
    std::atomic<size_t> thrown_{0};
    std::atomic<bool> stopped_{false};
};

/**
 * @brief MID node that appends a tag to frame_meta::description.
 *
 * Lets the pipeline smoke test verify that each stage actually ran, and in order.
 */
class tagging_mid_node : public cvedix_nodes::cvedix_node {
public:
    tagging_mid_node(const std::string& name, std::string tag)
        : cvedix_nodes::cvedix_node(name), tag_(std::move(tag)) {
        initialized();
    }

    ~tagging_mid_node() override { stop(); }

    void stop() {
        if (!stopped_.exchange(true)) {
            deinitialized();
        }
    }

    cvedix_nodes::cvedix_node_type node_type() override { return cvedix_nodes::cvedix_node_type::MID; }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        meta->description += tag_;
        return meta;
    }

private:
    std::string tag_;
    std::atomic<bool> stopped_{false};
};

/// @brief Build a minimal valid frame meta (frame_meta asserts on an empty Mat).
inline std::shared_ptr<cvedix_objects::cvedix_frame_meta> make_frame(int frame_index,
                                                                    int channel_index = 0) {
    cv::Mat frame(4, 4, CV_8UC3, cv::Scalar(0, 0, 0));
    return std::make_shared<cvedix_objects::cvedix_frame_meta>(frame, frame_index, channel_index, 4, 4, 25);
}

}  // namespace cvedix_test
