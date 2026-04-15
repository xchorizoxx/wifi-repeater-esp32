#pragma once

#include <cstring>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "StatusLed.hpp"

/**
 * @brief EventGroup bits for inter-task signalling.
 */
constexpr EventBits_t WIFI_CONNECTED_BIT   = BIT1;
constexpr EventBits_t WIFI_DISCONNECTED_BIT = BIT2;

/**
 * @brief Manages WiFi STA + AP coexistence, delegates NAPT to NaptManager,
 *        and provides periodic traffic-delta reporting.
 */
class WifiManager {
public:
    /**
     * @param led            Reference to StatusLed for state feedback.
     * @param trafficQueue   Queue to push uint32_t byte-delta values to LED task.
     * @param eventGroup     EventGroup shared across network tasks.
     */
    WifiManager(StatusLed& led, QueueHandle_t trafficQueue, EventGroupHandle_t eventGroup);

    ~WifiManager();

    // Non-copyable
    WifiManager(const WifiManager&)            = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /**
     * @brief Initialise netif, WiFi driver. Loads settings from ConfigManager
     *        and either starts STA+AP or AP-only mode.
     */
    void init();

    /**
     * @brief Trigger a WiFi scan. Results are placed into the provided buffer.
     * @param apRecords  Output array of scan results.
     * @param count      In: max records; Out: actual count.
     * @return ESP_OK on success.
     */
    esp_err_t scanNetworks(wifi_ap_record_t* apRecords, uint16_t* count);

    /**
     * @brief Clear saved credentials from ConfigManager and restart the device.
     */
    void clearCredentials();

private:
    static constexpr const char* TAG = "WifiMgr";

    // --- BOOT Button ---
    static constexpr gpio_num_t BOOT_PIN = GPIO_NUM_0;

    // --- Traffic monitor interval ---
    static constexpr uint32_t TRAFFIC_POLL_MS = 2000;

    // --- References ---
    StatusLed&          m_led;
    QueueHandle_t       m_trafficQueue;
    EventGroupHandle_t  m_eventGroup;

    // --- Netif handles ---
    esp_netif_t*        m_staNetif = nullptr;
    esp_netif_t*        m_apNetif  = nullptr;

    // --- State ---
    std::atomic<bool>   m_staConnected{false};
    uint32_t            m_lastBytes = 0;

    // --- Internal helpers ---
    void startAP();
    void connectSTA();

    // --- Event handler ---
    static void eventHandler(void* arg, esp_event_base_t base, int32_t id, void* eventData);
    void handleWifiEvent(int32_t id, void* eventData);
    void handleIpEvent(int32_t id, void* eventData);

    // --- Tasks ---
    static void trafficTaskWrapper(void* pvParameters);
    void trafficMonitorRun();

    static void bootTaskWrapper(void* pvParameters);
    void bootButtonRun();
};
