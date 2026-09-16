#pragma once

/**
 * @file sample_options.h
 * @brief Resolve sample inputs (media, model, labels, endpoints) from CLI args
 *        and environment variables instead of hardcoded absolute paths.
 *
 * Samples used to embed developer-machine paths such as
 * `/home/cvedix/cvedix_data/yolo12n.engine` and a production RTMP endpoint as
 * defaults. That made every sample unrunnable on a fresh checkout and risked
 * pushing test video to a live server. Resolution order is now:
 *
 *   1. explicit CLI flag  (`--video`, `--model`, `--labels`, `--rtmp`, ...)
 *   2. environment variable (`CVEDIX_VIDEO`, `CVEDIX_MODEL`, ...)
 *   3. a path relative to `CVEDIX_DATA_DIR` (default: `./cvedix_data`)
 *
 * Nothing points at a remote host by default: RTMP falls back to localhost.
 */

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace sample_options {

/**
 * @brief Read an environment variable, returning `fallback` when unset/empty.
 */
inline std::string env_or(const char* name, const std::string& fallback = "") {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

/**
 * @brief Root directory holding models, labels and test media.
 *
 * Override with `CVEDIX_DATA_DIR`. Defaults to `./cvedix_data` so a sample run
 * from the repository root works without editing source.
 */
inline std::string data_dir() {
    return env_or("CVEDIX_DATA_DIR", "cvedix_data");
}

/**
 * @brief Join `data_dir()` with a relative asset name.
 */
inline std::string data_path(const std::string& relative) {
    return (std::filesystem::path(data_dir()) / relative).string();
}

/**
 * @brief Resolve a value from CLI flag, then environment, then default.
 *
 * @param argc,argv       Raw program arguments.
 * @param flag            Long flag to look for, e.g. "--model".
 * @param env_name        Environment variable checked when the flag is absent.
 * @param default_value   Used when neither flag nor environment is set.
 */
inline std::string resolve(int argc, char** argv,
                           const char* flag,
                           const char* env_name,
                           const std::string& default_value) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == flag) {
            return argv[i + 1];
        }
    }
    return env_or(env_name, default_value);
}

/**
 * @brief Warn (once, on stderr) when a required input file is missing.
 *
 * Returns false so callers can bail out with a clear message instead of letting
 * a node fail deep inside GStreamer/TensorRT with an opaque error.
 */
inline bool check_exists(const std::string& label, const std::string& path) {
    if (path.empty()) {
        std::cerr << "[sample] missing " << label << ": no path provided\n";
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::cerr << "[sample] missing " << label << ": " << path << "\n"
                  << "         set it with --" << label << " <path> or CVEDIX_DATA_DIR=<dir>\n";
        return false;
    }
    return true;
}

}  // namespace sample_options
