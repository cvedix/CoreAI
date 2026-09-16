#include "cvedix_logger.h"
#include "cvedix/excepts/cvedix_invalid_calling_error.h"
#include <cstdlib>

namespace cvedix_utils {
        
    cvedix_logger::cvedix_logger(/* args */)
    {
    }

    cvedix_logger& cvedix_logger::get_logger() {
        static cvedix_logger logger;
        return logger;
    }
    
    cvedix_logger::~cvedix_logger() {
        shutdown();
    }

    void cvedix_logger::die() {
        alive = false;
        std::lock_guard<std::mutex> guard(log_cache_mutex);
        log_cache.push("die");
        log_cache_semaphore.signal();
    }

    void cvedix_logger::init() {
        std::lock_guard<std::mutex> guard(init_mutex);
        if (inited) {
            return;
        }
        inited = true;

        // initialize file writer
        file_writer.init(log_dir, log_file_name_template);

        // Register static cleanup function to run before static destructors
        std::atexit([]() {
            cvedix_logger::get_logger().shutdown();
        });

        // run thread
        auto t = std::thread(&cvedix_logger::log_write_run, this); 
        log_writer_th = std::move(t);
    }

    void cvedix_logger::shutdown() {
        std::lock_guard<std::mutex> guard(init_mutex);
        if (!inited) {
            return;
        }
        die();
        if (log_writer_th.joinable()) {
            log_writer_th.join();
        }

        #ifdef CVEDIX_WITH_KAFKA
        kafka_writer.shutdown();
        #endif

        inited = false;
    }

    void cvedix_logger::log(cvedix_log_level level, const std::string& message, const char* code_file, int code_line) {
        // make sure logger is initialized
        if (!inited) {
            // a bare `throw "..."` cannot be caught by `catch (const std::exception&)`,
            // so it escapes every handler in the pipeline and terminates the process.
            throw cvedix_excepts::cvedix_invalid_calling_error(
                "cvedix_logger is not initialized yet! call CVEDIX_LOGGER_INIT() first.");
        }
        
        // level filter
        if (level > log_level) {
            return;
        }
        
        // keywords filter for debug level
        if (level == cvedix_log_level::DEBUG && keywords_for_debug_log.size() != 0) {
            bool filterd = true;
            for(auto& keywords: keywords_for_debug_log) {
                if (message.find(keywords) != std::string::npos) {
                    filterd = false;
                    break;
                }
            }
            if (filterd) {
                return;
            }
        }
        
        /* create log */
        std::string new_log = "";
        // 100% true for log time
        if (include_time) {
            new_log += cvedix_utils::time_format(NOW, log_time_templete);
        }

        // log level
        if (include_level) {
            new_log += "[" + log_level_names.at(level) + "]";
        }

        // thread id
        if (include_thread_id) {
            auto id = std::this_thread::get_id();
            std::stringstream ss;
            ss << std::hex << id;  // to hex
            auto thread_id = ss.str(); 
            new_log += "[" + thread_id + "]";
        }
        
        // code location
        if (include_code_location) {
            new_log += "[" + std::string(code_file) + ":" + std::to_string(code_line) + "]";
        }

        new_log += " " + message;

        /* write to cache */
        // min lock range
        std::lock_guard<std::mutex> guard(log_cache_mutex);
        log_cache.push(new_log);
        // notify
        log_cache_semaphore.signal();
    }

    void cvedix_logger::log_write_run() {
        bool log_thres_warned = false;
        /* below code runs in single thread */
        while (inited && alive) {
            // wait for data
            log_cache_semaphore.wait();

            std::string log;
            int log_cache_size = 0;
            {
                // front()/pop() MUST be done under the lock: log() pushes from any
                // number of producer threads, and touching std::queue concurrently
                // is UB. Sample the remaining depth in the same critical section
                // instead of re-locking afterwards.
                std::lock_guard<std::mutex> guard(log_cache_mutex);
                // defensive: never call front() on an empty queue
                if (log_cache.empty()) {
                    continue;
                }
                log = log_cache.front();
                log_cache.pop();
                log_cache_size = static_cast<int>(log_cache.size());
            }

            if (log == "die") {
                continue;
            }

            /* watch the log cache size */
            // NOTE: CVEDIX_WARN re-enters log(), which takes log_cache_mutex, so
            // this must stay outside the critical section above.
            if (!log_thres_warned && log_cache_size > log_cache_warn_threshold) {
                CVEDIX_WARN(cvedix_utils::string_format("[logger] log cache size is exceeding threshold! cache size is: [%d], threshold is: [%d]", log_cache_size, log_cache_warn_threshold));
                log_thres_warned = true;  // warn 1 time
            }
            if (log_cache_size <= log_cache_warn_threshold) {
                log_thres_warned = false;
            }
            
            /* write to devices */
            if (log_to_console) {
                write_to_console(log);
            }
            
            if (log_to_file) {
                write_to_file(log);
            }

            if (log_to_kafka) {
                write_to_kafka(log);
            }
        }
    }

    void cvedix_logger::write_to_console(const std::string& log) {
        std::cout << log << std::endl;
    }

    void cvedix_logger::write_to_file(const std::string& log) {
        // file_writer.write(log);
        file_writer << log;
    }

    void cvedix_logger::write_to_kafka(const std::string& log) {
        #ifdef CVEDIX_WITH_KAFKA
        if (!kafka_writer.is_inited()) {
            auto servers_and_topic = cvedix_utils::string_split(kafka_servers_and_topic, '/');
            assert(servers_and_topic.size() == 2);
            kafka_writer.init(servers_and_topic[0], servers_and_topic[1]);
        }
        // kafka_writer.write(log);
        kafka_writer << log;
        #endif
    }
}
