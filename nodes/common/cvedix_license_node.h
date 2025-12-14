/**
 * @file cvedix_license_node.h
 * @brief Node hỗ trợ lấy thông tin hardware và quản lý license
 * 
 * Node này cung cấp các chức năng:
 * - Lấy Hardware ID của máy hiện tại
 * - Kiểm tra trạng thái license
 * - Lấy thông tin license (expiration, features, etc.)
 * - Hỗ trợ tạo license request
 * 
 * @example
 *   // Lấy hardware ID
 *   auto license_node = std::make_shared<cvedix_license_node>("license");
 *   std::string hw_id = license_node->get_hardware_id();
 *   
 *   // Kiểm tra license
 *   if (license_node->is_licensed()) {
 *       auto info = license_node->get_license_info();
 *       std::cout << "Expires: " << info.expiration << std::endl;
 *   }
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace cvedix_nodes {

/**
 * @brief License information structure
 */
struct LicenseInfo {
    bool valid = false;
    std::string expiration;
    std::string features;
    std::string version;
    std::string application;
    std::string hardware_id;
    bool hardware_bound = false;
    std::string error_message;
};

/**
 * @brief Hardware information structure
 */
struct HardwareInfo {
    std::string hash;           // Hardware hash (for license binding)
    std::string source_text;    // Detailed hardware info
    std::string cpu_info;
    std::string disk_info;
    std::string mac_address;
};

/**
 * @brief License request structure for generating new license
 */
struct LicenseRequest {
    std::string hardware_id;
    std::string requested_features;
    std::string requested_expiration;
    std::string contact_email;
    std::string company_name;
};

/**
 * @brief Node hỗ trợ quản lý license và hardware ID
 */
class cvedix_license_node {
private:
    std::string node_name_;
    std::string license_path_;
    bool initialized_ = false;
    LicenseInfo cached_license_info_;
    HardwareInfo cached_hardware_info_;
    
    // Helper methods
    bool load_license_info();
    bool load_hardware_info();
    std::string find_license_path();
    
public:
    /**
     * @brief Constructor
     * @param node_name Tên node
     * @param license_path Đường dẫn đến file license (optional)
     */
    explicit cvedix_license_node(
        const std::string& node_name,
        const std::string& license_path = ""
    );
    
    ~cvedix_license_node() = default;
    
    // ========================================
    // Hardware ID Functions
    // ========================================
    
    /**
     * @brief Lấy Hardware ID của máy hiện tại
     * @return Hardware hash (base64 encoded), dùng cho license binding
     */
    std::string get_hardware_id();
    
    /**
     * @brief Lấy thông tin hardware chi tiết
     * @return HardwareInfo structure
     */
    HardwareInfo get_hardware_info();
    
    /**
     * @brief In thông tin hardware (verbose mode)
     */
    void print_hardware_info();
    
    // ========================================
    // License Functions  
    // ========================================
    
    /**
     * @brief Kiểm tra license có hợp lệ không
     * @return true nếu license hợp lệ
     */
    bool is_licensed();
    
    /**
     * @brief Kiểm tra license và refresh cache
     * @return true nếu license hợp lệ
     */
    bool check_license();
    
    /**
     * @brief Lấy thông tin license
     * @return LicenseInfo structure
     */
    LicenseInfo get_license_info();
    
    /**
     * @brief Lấy đường dẫn file license đang sử dụng
     * @return Đường dẫn file license
     */
    std::string get_license_path();
    
    /**
     * @brief Kiểm tra feature có được license không
     * @param feature_name Tên feature (e.g., "tensorrt", "insightface")
     * @return true nếu feature được license
     */
    bool has_feature(const std::string& feature_name);
    
    /**
     * @brief Lấy danh sách features được license
     * @return Vector các tên features
     */
    std::vector<std::string> get_licensed_features();
    
    /**
     * @brief In thông tin license
     */
    void print_license_info();
    
    // ========================================
    // License Request Functions
    // ========================================
    
    /**
     * @brief Tạo license request để gửi cho nhà cung cấp
     * @param request Thông tin request
     * @return JSON string của request
     */
    std::string create_license_request(const LicenseRequest& request);
    
    /**
     * @brief Tạo license request đơn giản (chỉ cần hardware ID)
     * @return JSON string của request
     */
    std::string create_simple_request();
    
    /**
     * @brief Lưu license request ra file
     * @param filepath Đường dẫn file output
     * @param request Thông tin request
     * @return true nếu thành công
     */
    bool save_license_request(const std::string& filepath, const LicenseRequest& request);
    
    // ========================================
    // Utility Functions
    // ========================================
    
    /**
     * @brief In tất cả thông tin (hardware + license)
     */
    void print_all_info();
    
    /**
     * @brief Lấy node name
     */
    std::string get_node_name() const { return node_name_; }
};

} // namespace cvedix_nodes
