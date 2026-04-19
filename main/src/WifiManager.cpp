#include "WifiManager.hpp"
#include "ConfigManager.hpp"
#include "NaptManager.hpp"
#include "esp_mac.h"
#include "esp_netif_net_stack.h"
#include "lwip/netif.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
WifiManager::WifiManager(StatusLed &led, QueueHandle_t trafficQueue, EventGroupHandle_t eventGroup)
    : m_led(led), m_trafficQueue(trafficQueue), m_eventGroup(eventGroup) {}

WifiManager::~WifiManager() {}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void WifiManager::init() {
  // ConfigManager is already initialized in main before this!
  
  // --- TCP/IP + Event loop ---
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // --- Create default netifs ---
  m_apNetif = esp_netif_create_default_wifi_ap();
  m_staNetif = esp_netif_create_default_wifi_sta();

  // --- WiFi init ---
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // --- Register event handlers ---
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::eventHandler, this, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, ESP_EVENT_ANY_ID, &WifiManager::eventHandler, this, nullptr));

  // --- Set mode STA+AP ---
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

  // --- Start AP ---
  startAP();

  // --- Start WiFi ---
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  // --- Check for saved credentials via ConfigManager ---
  std::string staSsid = ConfigManager::getInstance().getStaSsid();
  if (!staSsid.empty()) {
    ESP_LOGI(TAG, "ConfigManager found STA credentials for '%s'", staSsid.c_str());
    connectSTA();
  } else {
    ESP_LOGI(TAG, "No STA credentials – standing by in AP-only mode");
    m_led.setState(LedState::STANDBY_BLUE);
  }

  // --- Start BOOT button task (GPIO 0 monitor) ---
  xTaskCreate(bootTaskWrapper, "boot_btn", 2048, this, 3, nullptr);
}

// ---------------------------------------------------------------------------
// AP configuration
// ---------------------------------------------------------------------------
void WifiManager::startAP() {
  ConfigManager& cfg = ConfigManager::getInstance();
  std::string apSsid = cfg.getApSsid();
  std::string apPass = cfg.getApPassword();

  wifi_config_t ap_config = {};
  std::strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), apSsid.c_str(), sizeof(ap_config.ap.ssid) - 1);
  std::strncpy(reinterpret_cast<char *>(ap_config.ap.password), apPass.c_str(), sizeof(ap_config.ap.password) - 1);
  ap_config.ap.ssid_len = static_cast<uint8_t>(apSsid.length());
  ap_config.ap.channel = 0;
  ap_config.ap.max_connection = 10;
  
  if (apPass.empty()) {
      ap_config.ap.authmode = WIFI_AUTH_OPEN;
  } else {
      ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_LOGI(TAG, "SoftAP configured: SSID='%s'", apSsid.c_str());
}

// ---------------------------------------------------------------------------
// STA connection
// ---------------------------------------------------------------------------
void WifiManager::connectSTA() {
  ConfigManager& cfg = ConfigManager::getInstance();
  std::string staSsid = cfg.getStaSsid();
  std::string staPass = cfg.getStaPassword();

  wifi_config_t sta_config = {};
  std::strncpy(reinterpret_cast<char *>(sta_config.sta.ssid), staSsid.c_str(), sizeof(sta_config.sta.ssid) - 1);
  std::strncpy(reinterpret_cast<char *>(sta_config.sta.password), staPass.c_str(), sizeof(sta_config.sta.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  
  // Note: Actual connection call is handled by Watchdog initially if it was disconnected, 
  // but we can call it here once for bootstrap.
  esp_wifi_connect();
  ESP_LOGI(TAG, "Connecting to STA '%s' ...", staSsid.c_str());
}


// ---------------------------------------------------------------------------
// Wipe Credentials
// ---------------------------------------------------------------------------
void WifiManager::clearCredentials() {
  ESP_LOGW(TAG, "Clearing WiFi credentials and restarting...");
  ConfigManager::getInstance().setStaConfig("", "");
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
}

// ---------------------------------------------------------------------------
// WiFi scan
// ---------------------------------------------------------------------------
esp_err_t WifiManager::scanNetworks(wifi_ap_record_t *apRecords, uint16_t *count) {
  xEventGroupSetBits(m_eventGroup, WIFI_SCANNING_BIT);

  std::string staSsid = ConfigManager::getInstance().getStaSsid();
  bool shouldReconnect = !staSsid.empty() && !m_staConnected.load(std::memory_order_relaxed);
  
  if (shouldReconnect) {
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  wifi_scan_config_t scanConf = {};
  scanConf.show_hidden = false;

  esp_err_t err = esp_wifi_scan_start(&scanConf, true); // blocking scan
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Scan failed: %d", err);
  } else {
    err = esp_wifi_scan_get_ap_records(count, apRecords);
  }

  xEventGroupClearBits(m_eventGroup, WIFI_SCANNING_BIT);

  // Resume reconnection if needed
  if (shouldReconnect) {
    esp_wifi_connect();
  }
  return err;
}

// ---------------------------------------------------------------------------
// Event handler (static → instance delegation)
// ---------------------------------------------------------------------------
void WifiManager::eventHandler(void *arg, esp_event_base_t base, int32_t id, void *eventData) {
  auto *self = static_cast<WifiManager *>(arg);
  if (base == WIFI_EVENT) {
    self->handleWifiEvent(id, eventData);
  } else if (base == IP_EVENT) {
    self->handleIpEvent(id, eventData);
  }
}

void WifiManager::handleWifiEvent(int32_t id, void *eventData) {
  switch (id) {
  case WIFI_EVENT_STA_DISCONNECTED:
    m_staConnected.store(false, std::memory_order_relaxed);
    xEventGroupClearBits(m_eventGroup, WIFI_CONNECTED_BIT);
    // ConnectionMonitor Watchdog will handle retries and exponential backoff!
    break;

  case WIFI_EVENT_AP_STACONNECTED: {
    auto *evt = static_cast<wifi_event_ap_staconnected_t *>(eventData);
    ESP_LOGI(TAG, "Client connected to AP – AID=%d", evt->aid);
    break;
  }
  case WIFI_EVENT_AP_STADISCONNECTED: {
    auto *evt = static_cast<wifi_event_ap_stadisconnected_t *>(eventData);
    ESP_LOGI(TAG, "Client disconnected from AP – AID=%d", evt->aid);
    break;
  }
  default:
    break;
  }
}

void WifiManager::handleIpEvent(int32_t id, void *eventData) {
  if (id == IP_EVENT_STA_GOT_IP) {
    auto *evt = static_cast<ip_event_got_ip_t *>(eventData);
    ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&evt->ip_info.ip));

    m_staConnected.store(true, std::memory_order_relaxed);
    xEventGroupSetBits(m_eventGroup, WIFI_CONNECTED_BIT);

    // Set STA as the default routing interface
    esp_netif_set_default_netif(m_staNetif);

    // Dynamically assign the DNS obtained from the home router to the AP DHCP
    esp_netif_dns_info_t dns;
    bool dns_found = false;

    if (esp_netif_get_dns_info(m_staNetif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK && dns.ip.u_addr.ip4.addr != 0) {
        ESP_LOGI(TAG, "DHCP: Using Router DNS: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
        dns_found = true;
    } else {
        ESP_LOGW(TAG, "DHCP: Router DNS not found, setting fallback 8.8.8.8");
        dns.ip.u_addr.ip4.addr = ipaddr_addr("8.8.8.8");
        dns.ip.type = IPADDR_TYPE_V4;
        dns_found = true;
    }

    if (dns_found) {
        uint8_t dhcps_offer_option = 0x02; // DHCPS_OFFER_DNS
        esp_netif_dhcps_stop(m_apNetif);
        esp_netif_dhcps_option(m_apNetif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option));
        esp_netif_set_dns_info(m_apNetif, ESP_NETIF_DNS_MAIN, &dns);
        esp_netif_dhcps_start(m_apNetif);
    }

    // Delegate NAPT to NaptManager
    NaptManager::getInstance().enableNapt(m_apNetif);
    
    m_led.setState(LedState::ACTIVE_GREEN);

    // Start traffic monitor if not already running
    if (m_trafficTaskHandle == nullptr) {
        xTaskCreate(trafficTaskWrapper, "traffic_mon", 3072, this, 2, &m_trafficTaskHandle);
    }
  }
}

// ---------------------------------------------------------------------------
// Traffic monitor task
// ---------------------------------------------------------------------------
void WifiManager::trafficTaskWrapper(void *pvParameters) {
  static_cast<WifiManager *>(pvParameters)->trafficMonitorRun();
}

void WifiManager::trafficMonitorRun() {
  m_lastBytes = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(TRAFFIC_POLL_MS));

    if (!m_staConnected.load(std::memory_order_relaxed))
      continue;

    esp_netif_t *netif = m_staNetif;
    if (!netif) continue;

    struct netif *lwip_nif = static_cast<struct netif *>(esp_netif_get_netif_impl(netif));
    if (!lwip_nif) continue;

    uint32_t currentBytes = 0;
#if MIB2_STATS
    currentBytes = lwip_nif->mib2_counters.ifinoctets + lwip_nif->mib2_counters.ifoutoctets;
#else
    static uint32_t fakeCounter = 0;
    fakeCounter += 100;
    currentBytes = fakeCounter;
#endif

    uint32_t delta = (currentBytes >= m_lastBytes) ? (currentBytes - m_lastBytes) : currentBytes; 
    m_lastBytes = currentBytes;
    xQueueOverwrite(m_trafficQueue, &delta);
  }
}

// ---------------------------------------------------------------------------
// BOOT button task (GPIO 0)
// ---------------------------------------------------------------------------
void WifiManager::bootTaskWrapper(void *pvParameters) {
  static_cast<WifiManager *>(pvParameters)->bootButtonRun();
}

void WifiManager::bootButtonRun() {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << BOOT_PIN);
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  uint32_t pressTime = 0;
  bool depressed = false;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));

    if (gpio_get_level(BOOT_PIN) == 0) {
      if (!depressed) {
        depressed = true;
        pressTime = 0;
      } else {
        pressTime += 100;
        if (pressTime >= 3000) {
          m_led.setState(LedState::ERROR_RED); 
          clearCredentials();                  // will restart device
        }
      }
    } else {
      depressed = false;
    }
  }
}
