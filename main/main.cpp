/**
 * @file main.cpp
 * @brief WiFi Repeater – ESP32-S3 entry point.
 *
 * Wires together StatusLed, WifiManager, and WebServer.
 * All objects are created once during init (heap-safe).
 * FreeRTOS tasks keep running after app_main returns.
 */

#include "StatusLed.hpp"
#include "WifiManager.hpp"
#include "WebServer.hpp"
#include "ConfigManager.hpp"
#include "ConnectionMonitor.hpp"
#include "esp_log.h"
#include "nvs_flash.h"

static constexpr const char* TAG = "main";

// --- GPIO for the on-board WS2812 LED ---
static constexpr gpio_num_t LED_GPIO = GPIO_NUM_48;

// --- Traffic delta queue (single-item overwrite queue) ---
static QueueHandle_t s_trafficQueue = nullptr;

// --- EventGroup for credential signalling ---
static EventGroupHandle_t s_eventGroup = nullptr;

// --- Statically-allocated object pointers (heap alloc once at init) ---
static StatusLed*         s_led             = nullptr;
static WifiManager*       s_wifiManager     = nullptr;
static WebServer*         s_webServer       = nullptr;
static ConnectionMonitor* s_connMonitor     = nullptr;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== WiFi Repeater starting ===");

    // 1. Create FreeRTOS primitives
    s_trafficQueue = xQueueCreate(1, sizeof(uint32_t));
    if (!s_trafficQueue) {
        ESP_LOGE(TAG, "Failed to create traffic queue");
        return;
    }

    s_eventGroup = xEventGroupCreate();
    if (!s_eventGroup) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // 2. Initialize ConfigManager (NVS Boot load)
    ConfigManager::getInstance().init();

    // 3. Create StatusLed (starts its own FreeRTOS task internally)
    s_led = new StatusLed(LED_GPIO, s_trafficQueue);
    ESP_LOGI(TAG, "StatusLed created on GPIO %d", static_cast<int>(LED_GPIO));

    // 4. Create WifiManager (brings up Netif & LwIP)
    s_wifiManager = new WifiManager(*s_led, s_trafficQueue, s_eventGroup);
    s_wifiManager->init();
    ESP_LOGI(TAG, "WifiManager initialised");

    // 5. Create & Start ConnectionMonitor (Watchdog)
    s_connMonitor = new ConnectionMonitor(s_eventGroup, WIFI_CONNECTED_BIT, *s_led);
    s_connMonitor->start();


    // 6. Create & start WebServer
    s_webServer = new WebServer(*s_wifiManager);
    esp_err_t err = s_webServer->start();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WebServer started (UI & OTA) – navigate to http://192.168.4.1");
    } else {
        ESP_LOGE(TAG, "WebServer failed to start");
    }

    ESP_LOGI(TAG, "=== Initialisation complete – tasks running ===");
    // app_main returns; FreeRTOS scheduler keeps all tasks alive.
}
