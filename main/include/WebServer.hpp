#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include "WifiManager.hpp"
#include <string>

/**
 * @brief OOP HTTP server for the WiFi Repeater config portal.
 *        Serves embedded HTML and handles config form submission.
 */
class WebServer {
public:
    WebServer(WifiManager& wifiMgr);
    ~WebServer();

    // Non-copyable
    WebServer(const WebServer&)            = delete;
    WebServer& operator=(const WebServer&) = delete;

    /**
     * @brief Start the HTTP server.
     * @return ESP_OK on success.
     */
    esp_err_t start();

    /**
     * @brief Stop the HTTP server.
     */
    void stop();

private:
    static constexpr const char* TAG = "WebSrv";

    WifiManager&   m_wifiMgr;
    httpd_handle_t m_server = nullptr;

    // --- URI handlers (static wrappers + context methods) ---
    
    // GET / (Main Page HTML)
    static esp_err_t getRootHandlerWrapper(httpd_req_t* req);
    esp_err_t getRootHandler(httpd_req_t* req);

    // POST /apply (Receives HTML Form data)
    static esp_err_t postApplyHandlerWrapper(httpd_req_t* req);
    esp_err_t postApplyHandler(httpd_req_t* req);

    // GET /scan (JSON List)
    static esp_err_t getScanHandlerWrapper(httpd_req_t* req);
    esp_err_t getScanHandler(httpd_req_t* req);

    // GET /status (System status JSON)
    static esp_err_t getStatusHandlerWrapper(httpd_req_t* req);
    esp_err_t getStatusHandler(httpd_req_t* req);

    // Helpers
    bool extractQueryParam(const char* query, const char* key, std::string& outValue);
};
