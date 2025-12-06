

#include "cvedix_log_file_writer.h"


namespace cvedix_utils {
        
    cvedix_log_file_writer::cvedix_log_file_writer() {

    }
    
    cvedix_log_file_writer::~cvedix_log_file_writer() {
        if (log_writer.is_open()) {
            log_writer.close();
        }
    }
    
    void cvedix_log_file_writer::init(std::string log_dir, std::string log_file_name_template) {
        this->log_dir = log_dir;
        this->log_file_name_template = log_file_name_template;
        
        // open log file first time
        auto f = create_valid_log_file_name();
        
        // delete old log files before creating new one
        delete_old_log_files(f);
        
        log_writer.open(f, std::ofstream::out | std::ofstream::app);

        inited = true;
    }


    void cvedix_log_file_writer::write(std::string log) {
        if (!inited) {
            throw "cvedix_log_file_writer not initialized!";
        }
        
        // check if need create new log file
        if (get_now_day() != log_day) {
            if (log_writer.is_open()) {
                log_writer.close();
            }

            auto f = create_valid_log_file_name();
            
            // delete old log files before creating new one
            delete_old_log_files(f);
            
            log_writer.open(f, std::ofstream::out | std::ofstream::app);
        }
        
        log_writer << log << std::endl;
    }

    std::string cvedix_log_file_writer::create_valid_log_file_name() {
        if (!std::experimental::filesystem::exists(log_dir)) {
            std::experimental::filesystem::create_directories(log_dir);
        }
        std::experimental::filesystem::path root_dir(log_dir);

        auto f_name = cvedix_utils::time_format(NOW, log_file_name_template);
        auto p = root_dir / f_name;
        
        // cache log start day
        log_day = get_now_day();

        return p.string();
    }

    int cvedix_log_file_writer::get_now_day() {
        std::vector<int> time_parts;
        cvedix_utils::time_split(NOW, time_parts);
        
        // refer to cvedix_utils::time_split(...), indice 2 is day
        return time_parts[2];
    }

    cvedix_log_file_writer& cvedix_log_file_writer::operator<<(std::string log) {
        write(log);
        return *this;
    }

    void cvedix_log_file_writer::delete_old_log_files(const std::string& current_log_file) {
        try {
            if (!std::experimental::filesystem::exists(log_dir)) {
                return;
            }

            std::experimental::filesystem::path current_path(current_log_file);
            std::string current_filename = current_path.filename().string();

            // Generate pattern to match log files based on template
            // For template like "<year>-<mon>-<day>.txt", we expect files like "2024-01-15.txt"
            // Use regex to match date pattern: YYYY-MM-DD.txt
            std::regex log_file_pattern(R"(\d{4}-\d{2}-\d{2}\.txt)");

            // Iterate through all files in log directory
            for (const auto& entry : std::experimental::filesystem::directory_iterator(log_dir)) {
                if (std::experimental::filesystem::is_regular_file(entry)) {
                    std::experimental::filesystem::path file_path = entry.path();
                    std::string filename = file_path.filename().string();

                    // Only delete log files that:
                    // 1. Match the log file pattern (YYYY-MM-DD.txt)
                    // 2. Are not the current log file
                    if (filename != current_filename && 
                        std::regex_match(filename, log_file_pattern)) {
                        try {
                            std::experimental::filesystem::remove(file_path);
                        } catch (const std::exception& e) {
                            // Ignore errors when deleting files (file might be in use, etc.)
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            // Ignore errors (directory might not exist, permission issues, etc.)
        }
    }
}