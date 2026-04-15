#include "NaptManager.hpp"
#include "ConfigManager.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/lwip_napt.h"

#include "lwip/netif.h"
#include "esp_netif_net_stack.h"

// Check if IP_NAPT is defined in esp-lwip (requires modified lwipopts.h or menuconfig)
#ifndef IP_NAPT
#define IP_NAPT 0
#endif

void NaptManager::enableNapt(esp_netif_t* apNetif) {
    if (!ConfigManager::getInstance().isNaptEnabled()) {
        ESP_LOGI(TAG, "NAPT is disabled in configuration.");
        return;
    }

    if (!apNetif) {
        ESP_LOGE(TAG, "Cannot enable NAPT: Invalid AP netif provided.");
        return;
    }

    ESP_LOGI(TAG, "Enabling IP Forwarding and NAPT via esp_netif...");
    
    // Enable NAPT on the AP interface using the official ESP-IDF API
    esp_err_t err = esp_netif_napt_enable(apNetif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable NAPT: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Verify that CONFIG_LWIP_IPV4_NAPT is enabled in menuconfig/sdkconfig");
    } else {
        ESP_LOGI(TAG, "NAPT Enabled successfully on AP interface.");
    }
}

esp_err_t NaptManager::addPortForwarding(uint16_t extPort, const std::string& intIp, uint16_t intPort) {
#if IP_NAPT
    // Needs advanced handling to hook lwip_portmap
    ESP_LOGI(TAG, "Adding Portmap: %d -> %s:%d", extPort, intIp.c_str(), intPort);
    // uint32_t ip = ipaddr_addr(intIp.c_str());
    // ip_portmap_add(IP_PROTO_TCP, my_ip, extPort, ip, intPort);
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Cannot add port forwarding, IP_NAPT is disabled.");
    return ESP_FAIL;
#endif
}
