#include "ConnectionMonitor.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "ConfigManager.hpp"
#include "WifiManager.hpp"
#include <algorithm>

ConnectionMonitor::ConnectionMonitor(EventGroupHandle_t eventGroup, 
                                     EventBits_t connectedBit, 
                                     StatusLed& led)
    : m_eventGroup(eventGroup), m_connectedBit(connectedBit), m_led(led) {
}

ConnectionMonitor::~ConnectionMonitor() {
    stop();
}

void ConnectionMonitor::start() {
    if (m_taskHandle == nullptr) {
        xTaskCreate(taskWrapper, "conn_monitor", 3072, this, 3, &m_taskHandle);
        ESP_LOGI(TAG, "ConnectionMonitor Watchdog started.");
    }
}

void ConnectionMonitor::stop() {
    if (m_taskHandle) {
        vTaskDelete(m_taskHandle);
        m_taskHandle = nullptr;
        ESP_LOGI(TAG, "ConnectionMonitor Watchdog stopped.");
    }
}

void ConnectionMonitor::taskWrapper(void* pvParameters) {
    auto* instance = static_cast<ConnectionMonitor*>(pvParameters);
    instance->run();
}

void ConnectionMonitor::run() {
    uint32_t currentBackoffMs = MIN_BACKOFF_MS;
    bool wasConnected = false;

    // Sleep for 10s initially to give the system time to boot and connect
    vTaskDelay(pdMS_TO_TICKS(10000));

    for (;;) {
        // Read the STA SSID to know if we even *should* be connected
        std::string staSsid = ConfigManager::getInstance().getStaSsid();
        if (staSsid.empty()) {
            // No target network configured, just sleep.
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        EventBits_t bits = xEventGroupGetBits(m_eventGroup);
        if ((bits & WIFI_SCANNING_BIT) != 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool isConnected = (bits & m_connectedBit) != 0;

        if (isConnected) {
            // Connection is currently healthy
            if (!wasConnected) {
                ESP_LOGI(TAG, "Connection healthy. Resetting backoff timer.");
                currentBackoffMs = MIN_BACKOFF_MS;
                wasConnected = true;
                
                // Only change LED if it was stuck in yellow from a reconnection attempt.
                // Normally WifiManager handles setting ACTIVE_GREEN on connect
                if (m_led.getState() == LedState::RECONNECTING_YELLOW) {
                    m_led.setState(LedState::ACTIVE_GREEN);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5000)); // Normal polling rate
        } else {
            // Disconnected!
            wasConnected = false;
            ESP_LOGW(TAG, "Watchdog detected disconnection from STA. Attempting to reconnect in %d ms...", currentBackoffMs);
            m_led.setState(LedState::RECONNECTING_YELLOW);

            // Wait the backoff period
            vTaskDelay(pdMS_TO_TICKS(currentBackoffMs));

            // Recheck if we are still disconnected before calling esp_wifi_connect (to avoid spam)
            bits = xEventGroupGetBits(m_eventGroup);
            if ((bits & m_connectedBit) == 0) {
                ESP_LOGI(TAG, "Executing esp_wifi_connect()...");
                esp_wifi_connect();
                
                // Exponential backoff
                currentBackoffMs *= 2;
                if (currentBackoffMs > MAX_BACKOFF_MS) {
                    currentBackoffMs = MAX_BACKOFF_MS;
                }
            }
        }
    }
}
