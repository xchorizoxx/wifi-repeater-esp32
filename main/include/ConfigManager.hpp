#pragma once

#include "esp_err.h"
#include <string>
#include <cstdint>

/**
 * @brief Singleton class to handle all Non-Volatile Storage (NVS) 
 *        operations efficiently. Reads configurations once on boot 
 *        into RAM, and exposes them.
 */
class ConfigManager {
public:
    // Singleton access
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // Initialize NVS and load all settings into memory
    void init();

    // --- Accessors for RAM Cache ---
    
    // AP Configuration
    std::string getApSsid() const { return m_apSsid; }
    std::string getApPassword() const { return m_apPassword; }
    
    // STA Configuration
    std::string getStaSsid() const { return m_staSsid; }
    std::string getStaPassword() const { return m_staPassword; }
    
    // NAPT & Advanced Config
    bool isNaptEnabled() const { return m_naptEnabled; }

    // --- Setters (Updates RAM and writes to NVS) ---
    esp_err_t setApConfig(const std::string& ssid, const std::string& password);
    esp_err_t setStaConfig(const std::string& ssid, const std::string& password);
    esp_err_t setNaptEnabled(bool enabled);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Default configuration constants
    static constexpr const char* DEFAULT_AP_SSID = "ESP32_WiFi_Repeater";
    static constexpr const char* DEFAULT_AP_PASS = "12345678";

    static constexpr const char* NVS_NAMESPACE = "router_cfg";

    // Memory cached variables
    std::string m_apSsid;
    std::string m_apPassword;
    std::string m_staSsid;
    std::string m_staPassword;
    bool m_naptEnabled = true;

    // Helper functions
    esp_err_t loadString(const char* key, std::string& outStr, const std::string& defaultVal);
    esp_err_t saveString(const char* key, const std::string& val);
    esp_err_t loadBool(const char* key, bool& outBool, bool defaultVal);
    esp_err_t saveBool(const char* key, bool val);
};
