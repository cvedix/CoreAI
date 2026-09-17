#include "test_assert.h"
#include "cvedix/nodes/ba/cvedix_ba_ppe_safety_node.h"
#include "cvedix/nodes/mid/cvedix_sync_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_app_des_node.h"
#include "cvedix/nodes/src/cvedix_app_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <mutex>

using namespace cvedix_nodes;
using namespace cvedix_objects;

namespace {
const ppe_observation person{{100, 100, 100, 300}, 100};
const ppe_observation helmet{{125, 100, 45, 40}, 0};
const ppe_observation vest{{110, 180, 80, 100}, 1};

class safety_probe : public cvedix_ba_ppe_safety_node {
public:
    safety_probe() : cvedix_ba_ppe_safety_node("safety_probe") {}
    using cvedix_ba_ppe_safety_node::handle_frame_meta;
};

auto make_meta(int frame, const std::vector<ppe_observation>& items) {
    auto meta = std::make_shared<cvedix_frame_meta>(cv::Mat::zeros(500, 500, CV_8UC3), frame, 0, 500, 500, 15);
    for (const auto& item : items) meta->targets.push_back(std::make_shared<cvedix_frame_target>(
        item.box.x, item.box.y, item.box.width, item.box.height, item.class_id, .9f, frame, 0));
    return meta;
}
}

CVEDIX_TEST_CASE(all_four_ppe_states_and_osd_colors) {
    safety_probe node;
    for (int mask = 0; mask < 4; ++mask) {
        std::vector<ppe_observation> items{person};
        if (!(mask & 1)) items.push_back(helmet);
        if (!(mask & 2)) items.push_back(vest);
        auto frame = make_meta(mask, items);
        node.handle_frame_meta(frame);
        CVEDIX_ASSERT_EQ(frame->targets[0]->secondary_class_ids.back(), mask);
        CVEDIX_ASSERT_EQ(frame->targets[0]->secondary_labels.back(), ppe_status_label(mask));
        const auto pixel = frame->osd_frame.at<cv::Vec3b>(350, 100);
        CVEDIX_ASSERT_EQ(pixel[1], mask ? 0 : 220);
        CVEDIX_ASSERT_EQ(pixel[2], mask ? 255 : 0);
        CVEDIX_ASSERT_EQ(cv::countNonZero(frame->frame.reshape(1)), 0);
    }
}

CVEDIX_TEST_CASE(ppe_items_cannot_make_two_overlapping_people_safe) {
    auto second = person; second.box.x += 25;
    const auto states = associate_ppe({person, second, helmet, vest});
    CVEDIX_ASSERT_EQ(states.size(), 2u);
    int helmets = 0, vests = 0, safe = 0;
    for (const auto& s : states) {
        helmets += s.helmet_index >= 0; vests += s.vest_index >= 0; safe += s.missing_mask() == 0;
    }
    CVEDIX_ASSERT_EQ(helmets, 1);
    CVEDIX_ASSERT_EQ(vests, 1);
    CVEDIX_ASSERT_LE(safe, 1);
}

CVEDIX_TEST_CASE(split_clones_original_not_the_already_dispatched_branch) {
    class split_probe : public cvedix_split_node {
    public:
        split_probe() : cvedix_split_node("isolated_split", false, true) {}
        using cvedix_split_node::push_meta;
    };
    class branch_probe : public cvedix_node {
        bool mutate;
    public:
        std::shared_ptr<cvedix_frame_meta> received;
        branch_probe(const std::string& name, bool mutate) : cvedix_node(name), mutate(mutate) { initialized(); }
        ~branch_probe() override { deinitialized(); }
        void meta_flow(std::shared_ptr<cvedix_meta> meta) override {
            received = std::dynamic_pointer_cast<cvedix_frame_meta>(meta);
            if (mutate) {
                received->description = "first branch changed this";
                received->targets[0]->primary_class_id = 999;
                received->frame.setTo(cv::Scalar(255, 255, 255));
            }
            cvedix_node::meta_flow(meta);
        }
    };
    auto split = std::make_shared<split_probe>();
    auto first = std::make_shared<branch_probe>("mutating_branch", true);
    auto second = std::make_shared<branch_probe>("observing_branch", false);
    struct cleanup {
        std::shared_ptr<cvedix_node> head;
        ~cleanup() { head->detach_recursively(); }
    } teardown{split};
    first->attach_to({split}); second->attach_to({split});
    auto original = make_meta(0, {person});
    split->push_meta(original);
    CVEDIX_ASSERT_EQ(first->received->targets[0]->primary_class_id, 999);
    CVEDIX_ASSERT_TRUE(second->received->description.empty());
    CVEDIX_ASSERT_EQ(second->received->targets[0]->primary_class_id, 100);
    CVEDIX_ASSERT_EQ(cv::countNonZero(second->received->frame.reshape(1)), 0);
    CVEDIX_ASSERT_EQ(original->targets[0]->primary_class_id, 100);
    CVEDIX_ASSERT_EQ(cv::countNonZero(original->frame.reshape(1)), 0);
}

CVEDIX_TEST_CASE(helmet_and_vest_boxes_visible_with_or_without_person) {
    safety_probe node;
    for (bool with_person : {true, false}) {
        auto frame = make_meta(0, with_person ? std::vector<ppe_observation>{person, helmet, vest}
                                            : std::vector<ppe_observation>{helmet, vest});
        if (with_person) frame->targets[0]->track_id = 42;
        node.handle_frame_meta(frame);
        const auto helmet_pixel = frame->osd_frame.at<cv::Vec3b>(130, 125);
        CVEDIX_ASSERT_EQ(helmet_pixel[0], 0);
        CVEDIX_ASSERT_EQ(helmet_pixel[1], 220);
        CVEDIX_ASSERT_EQ(helmet_pixel[2], 255);
        const auto vest_pixel = frame->osd_frame.at<cv::Vec3b>(260, 110);
        CVEDIX_ASSERT_EQ(vest_pixel[0], 255);
        CVEDIX_ASSERT_EQ(vest_pixel[1], 200);
        CVEDIX_ASSERT_EQ(vest_pixel[2], 0);
        if (with_person) CVEDIX_ASSERT_EQ(frame->targets[0]->track_id, 42);
    }
}

CVEDIX_TEST_CASE(bytetrack_filters_ppe_preserves_ids_and_recovers_short_occlusion) {
    std::mutex mutex;
    std::vector<std::shared_ptr<cvedix_frame_meta>> received;
    auto source = std::make_shared<cvedix_app_src_node>("track_source", 0);
    auto tracker = std::make_shared<cvedix_bytetrack_node>("test_bytetrack", cvedix_track_for::NORMAL,
        .35f, .35f, .8f, 60, 15, std::set<int>{100});
    auto sink = std::make_shared<cvedix_app_des_node>("track_output", 0);
    struct cleanup {
        std::shared_ptr<cvedix_node> head;
        ~cleanup() { head->detach_recursively(); }
    } teardown{source};
    sink->set_app_des_result_hooker([&](const std::string&, auto meta) {
        auto frame = std::dynamic_pointer_cast<cvedix_frame_meta>(meta);
        if (!frame) return;
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(frame);
    });
    tracker->attach_to({source}); sink->attach_to({tracker});
    cvedix_utils::cvedix_analysis_board board({source}, "");
    const auto before = board.snapshot();
    source->start();
    auto other = person; other.box.x = 300;
    int left_id = -1, right_id = -1;
    for (int i = 0; i < 6; ++i) {
        auto items = i == 4 ? std::vector<ppe_observation>{} :
            i == 2 ? std::vector<ppe_observation>{helmet, other, vest} :
            i % 2 ? std::vector<ppe_observation>{other, helmet, person, vest} :
                    std::vector<ppe_observation>{helmet, person, vest, other};
        auto frame = make_meta(i, items);
        for (auto& t : frame->targets) if (t->primary_class_id == 100) {
            t->x += i;
            // Below BYTETracker's built-in 0.6 default: exercises ctor settings.
            t->primary_score = .4f;
        }
        source->meta_flow(frame);
        CVEDIX_ASSERT_TRUE(cvedix_test::wait_for([&] {
            std::lock_guard<std::mutex> lock(mutex);
            return received.size() == static_cast<size_t>(i + 1);
        }));
        std::lock_guard<std::mutex> lock(mutex);
        std::set<int> ids;
        for (const auto& t : received.back()->targets) {
            if (t->primary_class_id != 100) {
                CVEDIX_ASSERT_EQ(t->track_id, -1);
                CVEDIX_ASSERT_TRUE(t->tracks.empty());
                continue;
            }
            CVEDIX_ASSERT_TRUE(t->track_id >= 0);
            CVEDIX_ASSERT_TRUE(ids.insert(t->track_id).second);
            auto& expected_id = t->x < 250 ? left_id : right_id;
            if (expected_id < 0) expected_id = t->track_id;
            CVEDIX_ASSERT_EQ(t->track_id, expected_id);
            CVEDIX_ASSERT_FALSE(t->tracks.empty());
        }
    }
    CVEDIX_ASSERT_TRUE(left_id != right_id);
    const auto after = board.snapshot();
    CVEDIX_ASSERT_EQ(before.size().width, after.size().width);
    CVEDIX_ASSERT_TRUE(cv::norm(before, after, cv::NORM_INF) > 0);
    source->stop();
}

CVEDIX_TEST_CASE(neighbor_and_floor_ppe_do_not_count) {
    auto nearby = helmet; nearby.box.x = 205;
    auto floor = helmet; floor.box.y = 370;
    auto low_vest = vest; low_vest.box.y = 340;
    const auto states = associate_ppe({person, nearby, floor, low_vest});
    CVEDIX_ASSERT_EQ(states[0].missing_mask(), 3);
    CVEDIX_ASSERT_TRUE(associate_ppe({helmet, vest}).empty());
}

CVEDIX_TEST_CASE(ppe_matching_resets_each_frame_and_rejects_stale_targets) {
    safety_probe node;
    auto complete = make_meta(0, {person, helmet, vest});
    node.handle_frame_meta(complete);
    auto next = make_meta(1, {person, helmet, vest});
    next->targets[1]->frame_index = 0;
    next->targets[2]->channel_index = 1;
    node.handle_frame_meta(next);
    CVEDIX_ASSERT_EQ(next->targets[0]->secondary_class_ids.back(), 3);
}

CVEDIX_TEST_CASE(two_branch_merge_waits_and_assesses_the_same_frame) {
    std::mutex mutex;
    std::vector<std::pair<int, int>> received;
    auto merge = std::make_shared<cvedix_sync_node>("test_merge", cvedix_sync_mode::MERGE, 60000);
    auto safety = std::make_shared<cvedix_ba_ppe_safety_node>("test_safety");
    auto sink = std::make_shared<cvedix_app_des_node>("test_output", 0);
    struct cleanup {
        std::shared_ptr<cvedix_node> head;
        ~cleanup() { head->detach_recursively(); }
    } teardown{merge};
    sink->set_app_des_result_hooker([&](const std::string&, auto meta) {
        auto frame = std::dynamic_pointer_cast<cvedix_frame_meta>(meta);
        if (!frame) return;
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& t : frame->targets) if (t->primary_class_id == 100)
            received.emplace_back(frame->frame_index, t->secondary_class_ids.back());
    });
    safety->attach_to({merge}); sink->attach_to({safety});
    for (int i = 0; i < 8; ++i) {
        auto people = make_meta(i, {person});
        auto ppe = make_meta(i, i % 2 ? std::vector<ppe_observation>{} : std::vector<ppe_observation>{helmet, vest});
        merge->meta_flow(i % 2 ? people : ppe);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        {
            std::lock_guard<std::mutex> lock(mutex);
            CVEDIX_ASSERT_EQ(received.size(), static_cast<size_t>(i));
        }
        merge->meta_flow(i % 2 ? ppe : people);
        CVEDIX_ASSERT_TRUE(cvedix_test::wait_for([&] {
            std::lock_guard<std::mutex> lock(mutex);
            return received.size() == static_cast<size_t>(i + 1);
        }));
        std::lock_guard<std::mutex> lock(mutex);
        CVEDIX_ASSERT_EQ(received.back().first, i);
        CVEDIX_ASSERT_EQ(received.back().second, i % 2 ? 3 : 0);
    }
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
    CVEDIX_SET_LOG_TO_FILE(false);
    CVEDIX_LOGGER_INIT();
    return cvedix_test::run_all(argc, argv);
}
