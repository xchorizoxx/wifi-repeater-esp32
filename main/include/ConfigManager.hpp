#pragma once

#include "esp_err.h"
#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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
    std::string getApSsid() const { xSemaphoreTake(m_mutex, portMAX_DELAY); std::string ret = m_apSsid; xSemaphoreGive(m_mutex); return ret; }
    std::string getApPassword() const { xSemaphoreTake(m_mutex, portMAX_DELAY); std::string ret = m_apPassword; xSemaphoreGive(m_mutex); return ret; }
    
    // STA Configuration
    std::string getStaSsid() const { xSemaphoreTake(m_mutex, portMAX_DELAY); std::string ret = m_staSsid; xSemaphoreGive(m_mutex); return ret; }
    std::string getStaPassword() const { xSemaphoreTake(m_mutex, portMAX_DELAY); std::string ret = m_staPassword; xSemaphoreGive(m_mutex); return ret; }
    
    // NAPT & Advanced Config
    bool isNaptEnabled() const { xSemaphoreTake(m_mutex, portMAX_DELAY); bool ret = m_naptEnabled; xSemaphoreGive(m_mutex); return ret; }

    // --- Setters (Updates RAM and writes to NVS) ---
    esp_err_t setApConfig(const std::string& ssid, const std::string& password);
    esp_err_t setStaConfig(const std::string& ssid, const std::string& password);
    esp_err_t setNaptEnabled(bool enabled);

private:
    ConfigManager() { m_mutex = xSemaphoreCreateMutex(); }
    ~ConfigManager() { if (m_mutex) vSemaphoreDelete(m_mutex); }

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Default configuration constants
    static constexpr const char* DEFAULT_AP_SSID = "ESP32_WiFi_Repeater";
    static constexpr const char* DEFAULT_AP_PASS = "12345678";

    static constexpr const char* NVS_NAMESPACE = "router_cfg";

    // Memory cached variables
    mutable SemaphoreHandle_t m_mutex = nullptr;
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
