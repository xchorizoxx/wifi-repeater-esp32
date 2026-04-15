#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Handles Over-The-Air (OTA) updates by receiving a `.bin` file
 *        via HTTP POST and writing it directly to the flash memory partition.
 */
class OtaUpdater {
public:
    static OtaUpdater& getInstance() {
        static OtaUpdater instance;
        return instance;
    }

    /**
     * @brief HTTP POST handler endpoint for receiving firmware chunks.
     */
    static esp_err_t postUpdateHandler(httpd_req_t* req);

private:
    OtaUpdater() = default;
    ~OtaUpdater() = default;

    OtaUpdater(const OtaUpdater&) = delete;
    OtaUpdater& operator=(const OtaUpdater&) = delete;

    static constexpr const char* TAG = "OtaUpdater";
};
