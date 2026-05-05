#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_sync_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <map>

// Custom multi-sync node
namespace {
class cvedix_multi_sync_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_multi_sync_node(std::string name, int expected_branches) 
        : cvedix_node(name), expected_branches_(expected_branches) {
        this->initialized();
    }
    
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        int fi = meta->frame_index;
        int ch = meta->channel_index;
        
        if (ch_cache.find(fi) == ch_cache.end()) {
            ch_cache[fi] = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta->clone());
            ch_cache[fi]->targets.clear();
            ch_cache[fi]->targets.insert(ch_cache[fi]->targets.end(), meta->targets.begin(), meta->targets.end());
            count_[ch][fi] = 1;
        } else {
            auto des = ch_cache[fi];
            des->targets.insert(des->targets.end(), meta->targets.begin(), meta->targets.end());
            count_[ch][fi]++;
        }
        
        if (count_[ch][fi] == expected_branches_) {
            auto result = ch_cache[fi];
            ch_cache.erase(fi);
            count_[ch].erase(fi);
            
            // Cleanup older frames that missed some branches
            for (auto it = ch_cache.begin(); it != ch_cache.end(); ) {
                if (fi - it->first > 15) { // 15 frames max queue
                    count_[ch].erase(it->first);
                    it = ch_cache.erase(it);
                } else {
                    ++it;
                }
            }
            
            pendding_meta(result);
        }
        return nullptr;
    }
private:
    int expected_branches_;
    std::mutex sync_mutex_;
    std::map<int, std::shared_ptr<cvedix_objects::cvedix_frame_meta>> ch_cache;
    std::map<int, std::map<int, int>> count_;
};
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Default Configuration
    std::string rtsp_url = "rtsp://admin:Admin1234@113.161.1.86:554/Streaming/Channels/101";
    int web_port = 9091;

    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  RTSP Vehicle & License Plate Detection Sample");
    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  RTSP: " + rtsp_url);
    CVEDIX_INFO("  Web:  http://localhost:" + std::to_string(web_port));

    // Models
    const std::string vehicle_engine = "./cvedix_data/models/yolo11n.engine";
    const std::string vehicle_labels = "./cvedix_data/models/yolov11/tensorrt/labels.txt";
    const std::string plate_engine = "./cvedix_data/models/license-plate-finetune-v1n.engine";
    const std::string plate_labels = "./cvedix_data/models/license_plate_labels.txt";

    // 1. RTSP Source Node
    auto rtsp_src = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src", 0, rtsp_url, 1.0f, "gstreamer", 0, "tcp"
    );

    // 2. Split Node (for 2 parallel branches: vehicle, plate)
    auto split_input = std::make_shared<cvedix_nodes::cvedix_split_node>("split_input", 2);

    // 3. Vehicle Detector (Branch 1)
    auto vehicle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "vehicle_detector", vehicle_engine, cvedix_nodes::YoloVersion::YOLO11, vehicle_labels, 
        0.40f, 0.45f, 0, cvedix_nodes::BackendType::TENSORRT
    );
    vehicle_detector->set_allowed_classes({2, 3, 5, 7}); // car, motorcycle, bus, truck

    // 4. Plate Detector (Branch 2)
    auto plate_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "plate_detector", plate_engine, cvedix_nodes::YoloVersion::YOLO11, plate_labels, 
        0.45f, 0.45f, 300, cvedix_nodes::BackendType::TENSORRT
    );
    plate_detector->set_allowed_classes({300});

    // 5. Multi-Sync Node (Merges 2 branches)
    auto sync = std::make_shared<cvedix_multi_sync_node>("multi_sync", 2);

    // 6. Tracker Node
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "tracker", cvedix_nodes::cvedix_track_for::NORMAL
    );

    // 7. OSD Node
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = true;
    osd_cfg.show_center_dot = true;
    osd_cfg.label_font_scale = 0.5;
    osd_cfg.enable_plate = true;
    osd->update_config(osd_cfg);

    // 8. Web Debug Destination
    auto web_des = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, web_port, nullptr, 60
    );

    // 9. Analysis Board
    auto board = std::make_unique<cvedix_utils::cvedix_analysis_board>(
        std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>{rtsp_src});
    board->push_to_buffer(5);
    web_des->set_board(board.get());

    // Connect Pipeline
    split_input->attach_to({rtsp_src});
    vehicle_detector->attach_to({split_input});
    plate_detector->attach_to({split_input});
    sync->attach_to({vehicle_detector, plate_detector});
    tracker->attach_to({sync});
    osd->attach_to({tracker});
    web_des->attach_to({osd});

    // Start Pipeline
    rtsp_src->start();

    std::string wait;
    std::getline(std::cin, wait);
    rtsp_src->detach_recursively();

    return 0;
}
