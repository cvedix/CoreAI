#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include "nodes/infers/base/cvedix_inference_interface.h"

namespace cvedix_utils {

    enum class HardwareType {
        CPU_ONLY,
        NVIDIA_GPU,
        ROCKCHIP_NPU,
        UNKNOWN
    };

    class cvedix_plugin_manager {
    public:
        static cvedix_plugin_manager& get_instance();

        /**
         * @brief Detects available hardware acceleration.
         * Checks for drivers/devices for NVIDIA, Rockchip, etc.
         */
        HardwareType detect_hardware();

        /**
         * @brief Load a specific backend plugin.
         * 
         * @param preferred_hw Hint to force a specific hardware backend. 
         *                     If UNKNOWN, uses auto-detection.
         * @return Pointer to the creating function interface, or nullptr on failure.
         */
        cvedix_nodes::CreateEngineFn load_plugin(HardwareType preferred_hw = HardwareType::UNKNOWN);

        /**
         * @brief Helper to destroy an engine instance using the correct plugin context.
         */
        void destroy_engine(cvedix_nodes::IInferenceEngine* engine);

    private:
        cvedix_plugin_manager();
        ~cvedix_plugin_manager();

        void* loaded_library_handle = nullptr;
        cvedix_nodes::CreateEngineFn create_fn = nullptr;
        cvedix_nodes::DestroyEngineFn destroy_fn = nullptr;
        
        HardwareType current_hw = HardwareType::UNKNOWN;
    };

}
