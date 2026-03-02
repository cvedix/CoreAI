#include "cvedix_plugin_manager.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace cvedix_utils {

    cvedix_plugin_manager& cvedix_plugin_manager::get_instance() {
        static cvedix_plugin_manager instance;
        return instance;
    }

    cvedix_plugin_manager::cvedix_plugin_manager() {}

    cvedix_plugin_manager::~cvedix_plugin_manager() {
        if (loaded_library_handle) {
            dlclose(loaded_library_handle);
            loaded_library_handle = nullptr;
        }
    }

    HardwareType cvedix_plugin_manager::detect_hardware() {
        if (current_hw != HardwareType::UNKNOWN) {
            return current_hw;
        }

        // 1. Check for Rockchip NPU
        // Common paths for RKNN drivers or device tree info
        if (std::filesystem::exists("/dev/rknn_npu") || 
            std::filesystem::exists("/dev/rknpu")) {
            CVEDIX_INFO("[PluginManager] Detected Rockchip NPU device file.");
            current_hw = HardwareType::ROCKCHIP_NPU;
            return current_hw;
        }

        // Check compatible string in device tree
        const std::string dt_compatible = "/proc/device-tree/compatible";
        if (std::filesystem::exists(dt_compatible)) {
            std::ifstream file(dt_compatible);
            if (file.good()) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (content.find("rockchip") != std::string::npos) {
                     // Check if NPU is actually present/enabled?
                     // For now, assume if it is a Rockchip board, we prefer RKNN if available.
                     // But some RK chips have no NPU. Let's stick to /dev checks for safer bet usually.
                     // However, /dev/rknn_npu is the standard one.
                     // Let's assume if it is rockchip but no /dev/rknn, maybe kernel driver not loaded?
                     // We will return CPU for now if no dev file, or check logic again.
                     // Actually, let's look for system libraries too.
                }
            }
        }

        // 2. Check for NVIDIA GPU
        if (std::filesystem::exists("/proc/driver/nvidia/version")) {
            CVEDIX_INFO("[PluginManager] Detected NVIDIA Driver in procfs.");
            current_hw = HardwareType::NVIDIA_GPU;
            return current_hw;
        }
        
        // Try command line
        int ret = std::system("which nvidia-smi > /dev/null 2>&1");
        if (ret == 0) {
            CVEDIX_INFO("[PluginManager] Detected nvidia-smi command.");
            current_hw = HardwareType::NVIDIA_GPU;
            return current_hw;
        }

        CVEDIX_INFO("[PluginManager] No specific accelerator detected. Defaulting to CPU.");
        current_hw = HardwareType::CPU_ONLY;
        return current_hw;
    }

    cvedix_nodes::CreateEngineFn cvedix_plugin_manager::load_plugin(HardwareType preferred_hw) {
        if (create_fn != nullptr) {
            return create_fn; // Already loaded
        }

        HardwareType hw = preferred_hw;
        if (hw == HardwareType::UNKNOWN) {
            hw = detect_hardware();
        }

        std::string lib_name;
        switch (hw) {
            case HardwareType::ROCKCHIP_NPU:
                lib_name = "libcvedix_backend_rknn.so";
                break;
            case HardwareType::NVIDIA_GPU:
                lib_name = "libcvedix_backend_trt.so";
                break;
            case HardwareType::CPU_ONLY:
            default:
                // Fallback or generic
                lib_name = "libcvedix_backend_cpu.so"; 
                break;
        }

        // Try to load
        // Paths to search: 
        // 1. Current directory/plugins
        // 2. /usr/lib/cvedix/plugins (AppImage standard)
        // 3. LD_LIBRARY_PATH
        
        std::vector<std::string> search_paths = {
            "./plugins/",
            "../lib/plugins/", // Standard AppImage structure often puts binary in bin/ and libs in lib/
            "/usr/lib/cvedix/plugins/",
            "" // rely on system path
        };

        // If running from AppImage, the executable is in usr/bin.
        // We want to look in usr/lib/plugins or usr/lib/cvedix/plugins
        // 'dirname(exe)/../lib/plugins' is a good guess.
        
        // Let's try loading
        for (const auto& path : search_paths) {
            std::string full_path = path + lib_name;
            if (path.empty()) full_path = lib_name;

            CVEDIX_INFO(cvedix_utils::string_format("[PluginManager] Trying to load plugin: %s", full_path.c_str()));
            
            loaded_library_handle = dlopen(full_path.c_str(), RTLD_LAZY);
            if (loaded_library_handle) {
                CVEDIX_INFO(cvedix_utils::string_format("[PluginManager] Successfully loaded: %s", full_path.c_str()));
                break;
            } else {
                CVEDIX_WARN(cvedix_utils::string_format("[PluginManager] Failed to load %s: %s", full_path.c_str(), dlerror()));
            }
        }

        if (!loaded_library_handle) {
            // If failed to load hardware specific, try fallback to CPU if we weren't already trying it
            if (hw != HardwareType::CPU_ONLY) {
                CVEDIX_WARN("[PluginManager] Failed to load hardware backend, attempting fallback to CPU backend...");
                return load_plugin(HardwareType::CPU_ONLY);
            }
            return nullptr;
        }

        // Load symbols
        create_fn = (cvedix_nodes::CreateEngineFn)dlsym(loaded_library_handle, "create_engine");
        destroy_fn = (cvedix_nodes::DestroyEngineFn)dlsym(loaded_library_handle, "destroy_engine");

        if (!create_fn || !destroy_fn) {
            CVEDIX_ERROR("[PluginManager] Plugin loaded but required symbols (create_engine/destroy_engine) are missing!");
            dlclose(loaded_library_handle);
            loaded_library_handle = nullptr;
            return nullptr;
        }

        return create_fn;
    }

    void cvedix_plugin_manager::destroy_engine(cvedix_nodes::IInferenceEngine* engine) {
        if (destroy_fn && engine) {
            destroy_fn(engine);
        } else if (engine) {
            // Dangerous fallback if plugin unloaded? assuming engine ptr is valid
            delete engine; 
        }
    }

}
