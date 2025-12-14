/**
 * @file face_recognition_mqtt_sample.cpp
 * @brief Face recognition with MQTT output sample (headless mode supported)
 * 
 * This sample reads video, detects and recognizes faces, then sends
 * recognition results as JSON to MQTT broker.
 * 
 * Usage:
 *   ./face_recognition_mqtt_sample <video_path> [database_path] [--headless]
 * 
 * Example:
 *   ./face_recognition_mqtt_sample ./video.mp4 ./face_database.txt           # With display
 *   ./face_recognition_mqtt_sample ./video.mp4 ./face_database.txt --headless # No display
 * 
 * MQTT Settings:
 *   Broker: anhoidong.datacenter.cvedix.com
 *   Port: 1883
 *   Topic: cvedix/face_recognition/results
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <csignal>

// Pipeline nodes
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_face_recognition_node.h"
#include "cvedix/nodes/broker/cvedix_json_mqtt_broker_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

// MQTT Client
#ifdef CVEDIX_WITH_MQTT
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#endif

// Analysis board
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

// MQTT Configuration
const std::string MQTT_BROKER = "anhoidong.datacenter.cvedix.com";
const int MQTT_PORT = 1883;
const std::string MQTT_TOPIC = "cvedix/face_recognition/results";
const std::string MQTT_CLIENT_ID = "cvedix_face_recognition_sample";

// Model paths (using non-int8 model to avoid compatibility issues)
const std::string MODEL_FACE_DETECTOR = "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx";
const std::string MODEL_FACE_RECOGNITION = "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx";

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    std::cout << "\n⏹️  Received signal " << signum << ", stopping...\n";
    g_running = false;
}

// Helper function to create timestamp string
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

// Custom JSON transformer to add metadata
std::string transform_json(const std::string& original_json) {
    std::stringstream ss;
    ss << "{";
    ss << "\"timestamp\": \"" << get_timestamp() << "\", ";
    ss << "\"source\": \"cvedix_face_recognition\", ";
    ss << "\"data\": " << original_json;
    ss << "}";
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Parse arguments
    if (argc < 2) {
        std::cout << "Face Recognition with MQTT Output Sample\n\n";
        std::cout << "Usage: " << argv[0] << " <video_path> [database_path] [--headless]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --headless    Run without display (for server/background processing)\n\n";
        std::cout << "Example:\n";
        std::cout << "  " << argv[0] << " ./video.mp4 ./face_database.txt\n";
        std::cout << "  " << argv[0] << " ./video.mp4 ./face_database.txt --headless\n\n";
        std::cout << "MQTT Settings:\n";
        std::cout << "  Broker: " << MQTT_BROKER << "\n";
        std::cout << "  Port:   " << MQTT_PORT << "\n";
        std::cout << "  Topic:  " << MQTT_TOPIC << "\n";
        return 1;
    }
    
    std::string video_path = argv[1];
    std::string database_path = "./face_database.txt";
    bool headless = false;
    
    // Parse optional arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "-h") {
            headless = true;
        } else if (arg[0] != '-') {
            database_path = arg;
        }
    }
    
    // Register signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize logging
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    
    std::cout << "\n========================================\n";
    std::cout << "🎬 Face Recognition MQTT Sample\n";
    std::cout << "========================================\n";
    std::cout << "Video:    " << video_path << "\n";
    std::cout << "Database: " << database_path << "\n";
    std::cout << "MQTT:     " << MQTT_BROKER << ":" << MQTT_PORT << "\n";
    std::cout << "Topic:    " << MQTT_TOPIC << "\n";
    std::cout << "Mode:     " << (headless ? "Headless (no display)" : "With display") << "\n";
    std::cout << "========================================\n\n";

#ifdef CVEDIX_WITH_MQTT
    // ==========================================
    // 1. Initialize MQTT Client
    // ==========================================
    std::cout << "📡 Connecting to MQTT broker...\n";
    
    auto mqtt_client = std::make_shared<cvedix_utils::cvedix_mqtt_client>(
        MQTT_BROKER,
        MQTT_PORT,
        MQTT_CLIENT_ID,
        60  // keepalive
    );
    
    // Set callbacks
    mqtt_client->set_on_connect_callback([](bool success) {
        if (success) {
            std::cout << "✅ Connected to MQTT broker\n";
        } else {
            std::cout << "❌ Failed to connect to MQTT broker\n";
        }
    });
    
    mqtt_client->set_on_disconnect_callback([]() {
        std::cout << "⚠️ Disconnected from MQTT broker\n";
    });
    
    // Enable auto-reconnect
    mqtt_client->set_auto_reconnect(true, 5000);
    
    // Connect (no auth for this broker)
    if (!mqtt_client->connect()) {
        std::cerr << "❌ Failed to initiate MQTT connection: " << mqtt_client->get_last_error() << "\n";
        return 1;
    }
    
    // Wait for connection
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!mqtt_client->is_connected()) {
        std::cerr << "⚠️ MQTT not connected yet, continuing anyway (will connect later)\n";
    }
    
    // Message counter for headless mode
    std::atomic<int> message_count{0};
    
    // MQTT publisher function for the broker node
    auto mqtt_publisher = [mqtt_client, &message_count](const std::string& json_message) {
        if (mqtt_client->is_connected()) {
            int result = mqtt_client->publish(MQTT_TOPIC, json_message, 1, false);
            if (result >= 0) {
                message_count++;
                // Print progress every 10 messages
                if (message_count % 10 == 0) {
                    std::cout << "📤 Sent " << message_count << " messages to MQTT (msg_id=" << result << ")\n";
                }
            } else {
                std::cerr << "❌ MQTT publish failed\n";
            }
        } else {
            CVEDIX_DEBUG("[MQTT] Not connected, message dropped");
        }
    };
#else
    // Fallback: just log to console if MQTT not available
    std::cout << "⚠️ MQTT not available, messages will be logged to console\n";
    std::atomic<int> message_count{0};
    auto mqtt_publisher = [&message_count](const std::string& json_message) {
        message_count++;
        std::cout << "[MQTT #" << message_count << "] " << json_message.substr(0, 150) << "...\n";
    };
#endif

    // ==========================================
    // 2. Create Pipeline Nodes
    // ==========================================
    std::cout << "\n🔧 Creating pipeline...\n";
    
    // Source: File input (video)
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src",
        0,              // channel_id
        video_path,
        0.2             // fps_scale (0.2x = slower playback for processing, reduce queue pressure)
    );
    std::cout << "  ✓ File source node\n";
    
    // Face Detector: YuNet
    auto face_detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "face_detector",
        MODEL_FACE_DETECTOR
    );
    std::cout << "  ✓ Face detector node (YuNet)\n";
    
    // Face Recognition: Using existing node with database
    auto face_recognition = std::make_shared<cvedix_nodes::cvedix_face_recognition_node>(
        "face_recognition",
        MODEL_FACE_RECOGNITION,
        database_path
    );
    std::cout << "  ✓ Face recognition node\n";
    
    // MQTT Broker: Serialize face data to JSON and publish via MQTT
    auto mqtt_broker = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
        "mqtt_broker",
        cvedix_nodes::cvedix_broke_for::FACE,  // Serialize face targets
        500,    // warn threshold (increased)
        2000,   // ignore threshold (increased to handle high FPS)
        transform_json,  // JSON transformer (adds timestamp)
        mqtt_publisher   // MQTT publisher function
    );
    // Increase queue size to handle high FPS video
    mqtt_broker->set_max_queue_size(500);
    std::cout << "  ✓ MQTT broker node (queue size: 500)\n";

    // ==========================================
    // 3. Build Pipeline (conditional based on headless mode)
    // ==========================================
    std::cout << "\n🔗 Building pipeline...\n";
    
    if (headless) {
        // Headless mode: No display
        // Pipeline: file_src → face_detector → face_recognition → mqtt_broker
        face_detector->attach_to({file_src});
        face_recognition->attach_to({face_detector});
        mqtt_broker->attach_to({face_recognition});
        
        std::cout << "  Pipeline (headless): file_src → face_detector → face_recognition → mqtt_broker\n";
    } else {
        // Display mode: Show video with OSD
        auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");
        auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);
        std::cout << "  ✓ OSD and Screen output nodes\n";
        
        face_detector->attach_to({file_src});
        face_recognition->attach_to({face_detector});
        mqtt_broker->attach_to({face_recognition});
        osd->attach_to({mqtt_broker});
        screen_des->attach_to({osd});
        
        std::cout << "  Pipeline: file_src → face_detector → face_recognition → mqtt_broker → osd → screen\n";
    }

    // ==========================================
    // 4. Start Pipeline
    // ==========================================
    std::cout << "\n▶️  Starting pipeline...\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    
    file_src->start();
    
    if (headless) {
        // Headless mode: Just wait until stopped or video ends
        std::cout << "🔄 Running in headless mode...\n";
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check if file source is still running
            // (You could add a check here if the node has a method to check status)
        }
        
        std::cout << "\n📊 Summary:\n";
        std::cout << "  Total messages sent: " << message_count << "\n";
    } else {
        // Display mode: Use analysis board
        cvedix_utils::cvedix_analysis_board board({file_src});
        board.display();
    }
    
#ifdef CVEDIX_WITH_MQTT
    // Disconnect MQTT on exit
    mqtt_client->disconnect();
    std::cout << "📡 Disconnected from MQTT broker\n";
#endif
    
    std::cout << "✅ Done!\n";
    return 0;
}
