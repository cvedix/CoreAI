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
    using create_backend_func = cvedix_infer_detector_backend* (*)(const char*);
    using destroy_backend_func = void (*)(cvedix_infer_detector_backend*);

    void* handle = nullptr;
    destroy_backend_func destroy_backend = nullptr;

public:
    cvedix_infer_detector_plugin_loader() = default;

    ~cvedix_infer_detector_plugin_loader() {
        if (handle) {
            dlclose(handle);
        }
    }

    /// @brief Load plugin from shared library and create a backend instance
    bool load(const std::string& plugin_path,
              const std::string& model_path,
              cvedix_infer_detector_backend*& backend) {
        unload(backend);
        dlerror(); // Clear previous error
        handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "[PluginLoader] Failed to load " << plugin_path
                      << ": " << dlerror() << std::endl;
            return false;
        }

        // Get factory function
        create_backend_func create = (create_backend_func)dlsym(handle, "create_backend");
        if (!create) {
            std::cerr << "[PluginLoader] Failed to find create_backend in " << plugin_path
                      << ": " << dlerror() << std::endl;
            dlclose(handle);
            handle = nullptr;
            return false;
        }

        destroy_backend = (destroy_backend_func)dlsym(handle, "destroy_backend");

        // Create backend (factory initializes it internally)
        backend = create(model_path.c_str());
        if (!backend) {
            std::cerr << "[PluginLoader] Failed to create backend from " << plugin_path << std::endl;
            dlclose(handle);
            handle = nullptr;
            destroy_backend = nullptr;
            return false;
        }

        return true;
    }

    /// @brief Destroy current backend instance and release the shared library
    void unload(cvedix_infer_detector_backend*& backend) {
        if (backend) {
            if (destroy_backend) {
                destroy_backend(backend);
            } else {
                backend->destroy();
                delete backend;
            }
            backend = nullptr;
        }

        if (handle) {
            dlclose(handle);
            handle = nullptr;
        }

        destroy_backend = nullptr;
    }
};

} // namespace cvedix::infers
