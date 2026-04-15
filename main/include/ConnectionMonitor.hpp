#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "StatusLed.hpp"

/**
 * @brief Watchdog task that monitors the WiFi STA connection.
 *        If the connection drops, it attempts reconnections using
 *        Exponential Backoff to avoid blocking the system.
 */
class ConnectionMonitor {
public:
    /**
     * @param eventGroup   Shared WiFi EventGroup holding WIFI_CONNECTED_BIT
     * @param connectedBit The exact bit flag for connection state
     * @param led          Reference to StatusLed (to set RECONNECTING_YELLOW)
     */
    ConnectionMonitor(EventGroupHandle_t eventGroup, 
                      EventBits_t connectedBit, 
                      StatusLed& led);

    ~ConnectionMonitor();

    // Non-copyable
    ConnectionMonitor(const ConnectionMonitor&) = delete;
    ConnectionMonitor& operator=(const ConnectionMonitor&) = delete;

    /**
     * @brief Start the background monitor task
     */
    void start();

    /**
     * @brief Stop the monitor task
     */
    void stop();

private:
    static constexpr const char* TAG = "ConnMonitor";
    
    // Backoff constants
    static constexpr uint32_t MIN_BACKOFF_MS = 5000;
    static constexpr uint32_t MAX_BACKOFF_MS = 60000;

    EventGroupHandle_t m_eventGroup;
    EventBits_t        m_connectedBit;
    StatusLed&         m_led;
    TaskHandle_t       m_taskHandle = nullptr;

    static void taskWrapper(void* pvParameters);
    void run();
};
