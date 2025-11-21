#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <iostream>
#include <chrono>
#include <thread>

/*
* ## sample for cvedix_logger ##
* show how cvedix_logger works
*/

int main() {
    // config for log content
    // CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    // CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    // CVEDIX_SET_LOG_INCLUDE_LEVEL(false);

    // config for output
    // CVEDIX_SET_LOG_TO_CONSOLE(false);
    // CVEDIX_SET_LOG_TO_FILE(false);

    // config for log folder
    CVEDIX_SET_LOG_DIR("./log");

    // config log level
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);

    // config for kafka, ignored automatically if not prepared for kafka
    CVEDIX_SET_LOG_TO_KAFKA(true);  // false by default if not set
    CVEDIX_SET_LOG_KAFKA_SERVERS_AND_TOPIC("192.168.77.87:9092/cvedix_log");
    
    // init
    CVEDIX_LOGGER_INIT();

    // 6 threads logging separately
    auto func1 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_ERROR(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        
    };

    auto func2 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_DEBUG(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(13));
        }
        
    };

    auto func3 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_INFO(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        
    };

    auto func4 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_WARN(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
    };

    auto func5 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_ERROR(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
    };

    auto func6 = []() {
        while (true) {
            /* code */
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;
            auto thread_id = ss.str(); 
            CVEDIX_INFO(cvedix_utils::string_format("thread id: %s", thread_id.c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
    };

    std::thread t1(func1);
    std::thread t2(func2);
    std::thread t3(func3);
    std::thread t4(func4);
    std::thread t5(func5);
    std::thread t6(func6);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();

    std::getchar();
}