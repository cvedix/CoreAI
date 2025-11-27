#pragma once

#include "../common/cvedix_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include <functional>

namespace cvedix_nodes {
    /**
     * Custom Data Transform Node
     * 
     * Node này cho phép tùy chỉnh dữ liệu frame_meta theo yêu cầu khách hàng
     * trước khi gửi đến các node tiếp theo (như broker).
     * 
     * Tính năng:
     * - Tùy chỉnh frame_meta (filter, transform, add/remove fields)
     * - Có thể filter targets theo điều kiện
     * - Có thể thêm/sửa/xóa thông tin trong targets
     * - Có thể modify frame data
     * 
     * Pipeline: Previous Node → Custom Transform → Next Node (Broker)
     * 
     * Ví dụ sử dụng:
     *   auto transform_node = std::make_shared<cvedix_custom_data_transform_node>(
     *       "custom_transform_0",
     *       [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
     *           // Filter targets với score > 0.5
     *           auto it = meta->targets.begin();
     *           while (it != meta->targets.end()) {
     *               if ((*it)->primary_score < 0.5) {
     *                   it = meta->targets.erase(it);
     *               } else {
     *                   ++it;
     *               }
     *           }
     *           return meta; // Return modified meta
     *       }
     *   );
     */
    class cvedix_custom_data_transform_node : public cvedix_node {
    private:
        // Custom transform function
        // Input: original frame_meta
        // Output: transformed frame_meta (can be same or modified)
        // Return nullptr to drop this frame (not forward to next nodes)
        std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> transform_func;
        
    protected:
        // Override handle_frame_meta to apply custom transformation
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> 
            handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        
    public:
        /**
         * Constructor
         * @param node_name: Tên node
         * @param transform_func: Function để tùy chỉnh frame_meta
         *   - Input: frame_meta gốc
         *   - Output: frame_meta đã tùy chỉnh (hoặc nullptr để drop frame)
         *   - Nếu nullptr, frame_meta sẽ được forward không thay đổi
         */
        cvedix_custom_data_transform_node(
            std::string node_name,
            std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
                std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> transform_func = nullptr);
        
        ~cvedix_custom_data_transform_node();
        
        /**
         * Set custom transform function
         */
        void set_transform_func(
            std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
                std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> func);
        
        /**
         * Get current transform function
         */
        std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> get_transform_func() const;
    };
}

