#include "ConfigManager.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char* TAG = "ConfigManager";

void ConfigManager::init() {
    ESP_LOGI(TAG, "Initializing ConfigManager and NVS...");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to recover...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Load Variables into Memory Cache
    loadString("ap_ssid", m_apSsid, DEFAULT_AP_SSID);
    loadString("ap_pass", m_apPassword, DEFAULT_AP_PASS);
    
    // STA can be empty by default, meaning we haven't configured a connection yet
    loadString("sta_ssid", m_staSsid, "");
    loadString("sta_pass", m_staPassword, "");

    loadBool("napt_en", m_naptEnabled, true);

    ESP_LOGI(TAG, "NVS Settings Loaded into RAM.");
}

esp_err_t ConfigManager::setApConfig(const std::string& ssid, const std::string& password) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (ssid == m_apSsid && password == m_apPassword) {
        xSemaphoreGive(m_mutex);
        return ESP_OK; // No change
    }
    
    esp_err_t err1 = saveString("ap_ssid", ssid);
    esp_err_t err2 = saveString("ap_pass", password);
    esp_err_t err = ESP_FAIL;
    
    if (err1 == ESP_OK && err2 == ESP_OK) {
        m_apSsid = ssid;
        m_apPassword = password;
        err = ESP_OK;
    }
    
    xSemaphoreGive(m_mutex);
    return err;
}

esp_err_t ConfigManager::setStaConfig(const std::string& ssid, const std::string& password) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (ssid == m_staSsid && password == m_staPassword) {
        xSemaphoreGive(m_mutex);
        return ESP_OK; // No change
    }

    esp_err_t err1 = saveString("sta_ssid", ssid);
    esp_err_t err2 = saveString("sta_pass", password);
    esp_err_t err = ESP_FAIL;
    
    if (err1 == ESP_OK && err2 == ESP_OK) {
        m_staSsid = ssid;
        m_staPassword = password;
        err = ESP_OK;
    }
    
    xSemaphoreGive(m_mutex);
    return err;
}

esp_err_t ConfigManager::setNaptEnabled(bool enabled) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (enabled == m_naptEnabled) {
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    esp_err_t err = saveBool("napt_en", enabled);
    if (err == ESP_OK) {
        m_naptEnabled = enabled;
    }
    xSemaphoreGive(m_mutex);
    return err;
}

// ---------------------------------------------------------
// Private Helpers for NVS Operations
// ---------------------------------------------------------

esp_err_t ConfigManager::loadString(const char* key, std::string& outStr, const std::string& defaultVal) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        outStr = defaultVal;
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_str(handle, key, nullptr, &required_size);
    if (err == ESP_OK && required_size > 0 && required_size <= 64) {
        char buf[64] = {};
        nvs_get_str(handle, key, buf, &required_size);
        outStr = std::string(buf);
    } else if (err == ESP_OK && required_size > 64) {
        ESP_LOGW(TAG, "Value for key %s too long (%d), using default", key, required_size);
        outStr = defaultVal;
    } else {
        outStr = defaultVal;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigManager::saveString(const char* key, const std::string& val) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, key, val.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    } else {
        ESP_LOGE(TAG, "Failed to save NVS key %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::loadBool(const char* key, bool& outBool, bool defaultVal) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        outBool = defaultVal;
        return err;
    }

    uint8_t val = 0;
    err = nvs_get_u8(handle, key, &val);
    if (err == ESP_OK) {
        outBool = (val != 0);
    } else {
        outBool = defaultVal;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigManager::saveBool(const char* key, bool val) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, key, val ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    } else {
        ESP_LOGE(TAG, "Failed to save NVS bool key %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}
