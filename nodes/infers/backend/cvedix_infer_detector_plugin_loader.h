#pragma once

#include "cvedix_infer_detector_backend.h"
#include <dlfcn.h>
#include <iostream>

namespace cvedix_nodes::infers {

/**
 * @brief Simple plugin loader using dlopen
 */
class cvedix_infer_detector_plugin_loader {
private:
    void* handle = nullptr;

public:
    cvedix_infer_detector_plugin_loader() = default;

    ~cvedix_infer_detector_plugin_loader() {
        if (handle) {
            dlclose(handle);
        }
    }

    /// @brief Load plugin from shared library
    /// @param plugin_path Path to .so file
    /// @param model_path Path to model file
    /// @return Pointer to cvedix_infer_detector_backend, or nullptr if failed
    static cvedix_infer_detector_backend* load(const std::string& plugin_path,
                                  const std::string& model_path) {
        dlerror(); // Clear previous error
        void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "[PluginLoader] Failed to load " << plugin_path
                      << ": " << dlerror() << std::endl;
            return nullptr;
        }

        // Get factory function
        typedef cvedix_infer_detector_backend* (*create_backend_func)(const char*);
        create_backend_func create = (create_backend_func)dlsym(handle, "create_backend");
        if (!create) {
            std::cerr << "[PluginLoader] Failed to find create_backend in " << plugin_path
                      << ": " << dlerror() << std::endl;
            dlclose(handle);
            return nullptr;
        }

        // Create backend (factory initializes it internally)
        cvedix_infer_detector_backend* backend = create(model_path.c_str());
        if (!backend) {
            std::cerr << "[PluginLoader] Failed to create backend from " << plugin_path << std::endl;
            dlclose(handle);
            return nullptr;
        }

        // Successfully loaded and created backend
        return backend;
    }
};

} // namespace cvedix::infers
