#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include <string>

/**
 * @brief Handles the initialization of the LwIP NAPT (Network Address and 
 *        Port Translation) features, including IP forwarding and optional port mapping.
 */
class NaptManager {
public:
    static NaptManager& getInstance() {
        static NaptManager instance;
        return instance;
    }

    /**
     * @brief Enables IP Forwarding and NAPT on the given AP interface
     * @param apNetif Pointer to the initialized AP network interface
     */
    void enableNapt(esp_netif_t* apNetif);

    /**
     * @brief Adds a basic port forwarding rule (requires IP_NAPT enabled in lwipopts)
     * @param extPort External port to listen on via the STA IP
     * @param intIp   Internal IP of the client connected to the AP
     * @param intPort Internal port of the client
     */
    esp_err_t addPortForwarding(uint16_t extPort, const std::string& intIp, uint16_t intPort);

private:
    NaptManager() = default;
    ~NaptManager() = default;

    NaptManager(const NaptManager&) = delete;
    NaptManager& operator=(const NaptManager&) = delete;

    static constexpr const char* TAG = "NaptManager";
};
