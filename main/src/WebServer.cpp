#include "WebServer.hpp"
#include "ConfigManager.hpp"
#include "OtaUpdater.hpp"
#include "esp_system.h"
#include "esp_wifi.h"
#include <vector>
#include <algorithm>
#include <cctype>

static char from_hex(char ch) {
    return isdigit((unsigned char)ch) ? ch - '0' : tolower((unsigned char)ch) - 'a' + 10;
}

static void urlDecode(std::string &str) {
    std::string ret;
    ret.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                ret += (char)(from_hex(str[i+1]) << 4 | from_hex(str[i+2]));
                i += 2;
            }
        } else if (str[i] == '+') {
            ret += ' ';
        } else {
            ret += str[i];
        }
    }
    str = ret;
}

// --- Static HTML Content ---
// Embedded HTML for the Configuration Portal
static const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>ESP32 Router Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root { --primary: #00d2ff; --secondary: #3a7bd5; --bg: #0f172a; --card: rgba(30, 41, 59, 0.7); --text: #f1f5f9; }
        body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: radial-gradient(circle at top right, #1e293b, #0f172a); color: var(--text); margin: 0; min-height: 100vh; display: flex; flex-direction: column; align-items: center; padding: 20px; }
        .glass { background: var(--card); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 16px; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3); }
        .container { width: 100%; max-width: 500px; padding: 24px; box-sizing: border-box; }
        h2, h3 { color: var(--primary); margin-top: 0; text-align: center; font-weight: 300; letter-spacing: 1px; }
        .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 24px; }
        .status-item { padding: 12px; text-align: center; }
        .status-label { font-size: 0.75rem; color: #94a3b8; text-transform: uppercase; margin-bottom: 4px; }
        .status-value { font-size: 1.1rem; font-weight: bold; }
        label { display: block; margin: 16px 0 6px; font-size: 0.9rem; color: #94a3b8; }
        input { width: 100%; padding: 12px; background: rgba(0,0,0,0.2); border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; color: white; box-sizing: border-box; transition: 0.3s; }
        input:focus { border-color: var(--primary); outline: none; box-shadow: 0 0 0 2px rgba(0, 210, 255, 0.2); }
        .btn { width: 100%; padding: 14px; border: none; border-radius: 8px; font-size: 1rem; font-weight: bold; cursor: pointer; transition: 0.3s; margin-top: 20px; }
        .btn-primary { background: linear-gradient(135deg, var(--primary), var(--secondary)); color: white; }
        .btn-primary:hover { transform: translateY(-2px); box-shadow: 0 4px 15px rgba(0, 210, 255, 0.3); }
        .btn-scan { background: rgba(255,255,255,0.05); color: var(--primary); border: 1px solid var(--primary); margin-top: 10px; }
        .scan-list { margin-top: 15px; max-height: 200px; overflow-y: auto; background: rgba(0,0,0,0.2); border-radius: 8px; }
        .scan-item { padding: 10px; border-bottom: 1px solid rgba(255,255,255,0.05); cursor: pointer; }
        .scan-item:hover { background: rgba(255,255,255,0.1); }
        .footer { margin-top: 30px; font-size: 0.8rem; color: #64748b; }
    </style>
</head>
<body>
    <div class="container glass">
        <h2>Router Config</h2>
        <div class="status-grid">
            <div class="status-item glass">
                <div class="status-label">Modo</div>
                <div class="status-value" id="val-mode">STA+AP</div>
            </div>
            <div class="status-item glass">
                <div class="status-label">Señal</div>
                <div class="status-value" id="val-rssi">-- dBm</div>
            </div>
        </div>

        <form action="/apply" method="POST">
            <h3>WiFi Origen (STA)</h3>
            <label>SSID del Router</label>
            <input type="text" name="sta_ssid" id="sta_ssid" placeholder="Selecciona o escribe..." required>
            <button type="button" class="btn btn-scan" id="btn-scan">Escanear Redes</button>
            <div class="scan-list" id="scan-list" style="display:none"></div>
            
            <label>Contraseña</label>
            <input type="password" name="sta_pass" placeholder="Contraseña de la red">
            
            <h3>Red del Repeater (AP)</h3>
            <label>SSID del Repetidor</label>
            <input type="text" name="ap_ssid" id="ap_ssid" placeholder="Ej. MiRepeater_Ext">
            
            <label>Contraseña AP</label>
            <input type="password" name="ap_pass" placeholder="Dejar en blanco para abierta">
            
            <input type="submit" class="btn btn-primary" value="Guardar y Reiniciar">
        </form>
    </div>

    <div class="footer">ESP32-S3 Firmware v2.0 | Senior OOP</div>

    <script>
        document.getElementById('btn-scan').onclick = async () => {
            const list = document.getElementById('scan-list');
            list.style.display = 'block';
            list.innerHTML = '<div style="padding:10px;text-align:center">Escaneando...</div>';
            try {
                const res = await fetch('/scan');
                const data = await res.json();
                list.innerHTML = '';
                data.forEach(ap => {
                    const div = document.createElement('div');
                    div.className = 'scan-item';
                    div.innerHTML = `<strong>${ap.ssid}</strong> <span style="font-size:0.8em;opacity:0.6">(${ap.rssi}dBm, Ch:${ap.chan})</span>`;
                    div.onclick = () => {
                        document.getElementById('sta_ssid').value = ap.ssid;
                        list.style.display = 'none';
                    };
                    list.appendChild(div);
                });
            } catch (e) {
                list.innerHTML = '<div style="padding:10px;color:red">Error al escanear</div>';
            }
        };

        // Update status values
        setInterval(async () => {
            try {
                const res = await fetch('/status');
                const data = await res.json();
                document.getElementById('val-rssi').innerText = data.rssi + ' dBm';
                document.getElementById('val-mode').innerText = data.connected ? 'Conectado' : 'Sin Internet';
            } catch (e) {}
        }, 5000);
    </script>
</body>
</html>
)rawliteral";

static const char* SUCCESS_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Guardado</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f9; display: flex; justify-content: center; align-items: center; height: 100vh; text-align: center; }
        .box { background: white; padding: 2rem; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        h2 { color: #28a745; }
    </style>
</head>
<body>
    <div class="box">
        <h2>Configuración Guardada</h2>
        <p>El dispositivo se reiniciará en unos segundos para aplicar los cambios.</p>
        <p>Por favor conéctate a la nueva red si cambiaste el nombre o contraseña del AP.</p>
    </div>
</body>
</html>
)rawliteral";


// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
WebServer::WebServer(WifiManager& wifiMgr) : m_wifiMgr(wifiMgr) {
}

WebServer::~WebServer() {
    stop();
}

// ---------------------------------------------------------------------------
// Server Control
// ---------------------------------------------------------------------------
esp_err_t WebServer::start() {
    if (m_server != nullptr) {
        return ESP_OK; // Already running
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Allow up to 6 URIs (/, /scan, /status, /apply, /update)
    config.max_uri_handlers = 6;
    config.stack_size = 4096;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    esp_err_t err = httpd_start(&m_server, &config);
    if (err == ESP_OK) {
        // Register GET /
        httpd_uri_t root_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = getRootHandlerWrapper,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(m_server, &root_uri));

        // Register GET /scan
        httpd_uri_t scan_uri = {
            .uri      = "/scan",
            .method   = HTTP_GET,
            .handler  = getScanHandlerWrapper,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(m_server, &scan_uri));

        // Register GET /status
        httpd_uri_t status_uri = {
            .uri      = "/status",
            .method   = HTTP_GET,
            .handler  = getStatusHandlerWrapper,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(m_server, &status_uri));

        // Register POST /apply
        httpd_uri_t apply_uri = {
            .uri      = "/apply",
            .method   = HTTP_POST,
            .handler  = postApplyHandlerWrapper,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(m_server, &apply_uri));

        // Register POST /update (OTA)
        httpd_uri_t update_uri = {
            .uri      = "/update",
            .method   = HTTP_POST,
            .handler  = OtaUpdater::postUpdateHandler,
            .user_ctx = nullptr
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(m_server, &update_uri));

        ESP_LOGI(TAG, "HTTP Server handlers registered.");
    } else {
        ESP_LOGE(TAG, "Error starting HTTP Server: %s", esp_err_to_name(err));
    }
    return err;
}

void WebServer::stop() {
    if (m_server) {
        httpd_stop(m_server);
        m_server = nullptr;
        ESP_LOGI(TAG, "HTTP Server stopped");
    }
}

// ---------------------------------------------------------------------------
// URI Handlers
// ---------------------------------------------------------------------------

esp_err_t WebServer::getRootHandlerWrapper(httpd_req_t* req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    return server->getRootHandler(req);
}

esp_err_t WebServer::getRootHandler(httpd_req_t* req) {
    // Standard HTML response with UTF-8
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebServer::getScanHandlerWrapper(httpd_req_t* req) {
    return static_cast<WebServer*>(req->user_ctx)->getScanHandler(req);
}

esp_err_t WebServer::getScanHandler(httpd_req_t* req) {
    wifi_ap_record_t ap_info[15];
    uint16_t ap_count = 15;
    
    // Perform the scan using our WifiManager instance
    esp_err_t err = m_wifiMgr.scanNetworks(ap_info, &ap_count);

    std::string json = "[";
    if (err == ESP_OK) {
        for (int i = 0; i < ap_count; i++) {
            json += "{";
            json += "\"ssid\":\"" + std::string((char*)ap_info[i].ssid) + "\",";
            json += "\"rssi\":" + std::to_string(ap_info[i].rssi) + ",";
            json += "\"chan\":" + std::to_string(ap_info[i].primary);
            json += "}";
            if (i < ap_count - 1) json += ",";
        }
    }
    json += "]";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

esp_err_t WebServer::getStatusHandlerWrapper(httpd_req_t* req) {
    return static_cast<WebServer*>(req->user_ctx)->getStatusHandler(req);
}

esp_err_t WebServer::getStatusHandler(httpd_req_t* req) {
    wifi_ap_record_t ap;
    int rssi = 0;
    bool connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    if (connected) rssi = ap.rssi;

    std::string json = "{";
    json += "\"connected\":" + std::string(connected ? "true" : "false") + ",";
    json += "\"rssi\":" + std::to_string(rssi);
    json += "}";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

esp_err_t WebServer::postApplyHandlerWrapper(httpd_req_t* req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    return server->postApplyHandler(req);
}

esp_err_t WebServer::postApplyHandler(httpd_req_t* req) {
    // Read the POST body
    int total_len = req->content_len;
    int cur_len = 0;
    std::vector<char> buf(total_len + 1, '\0');

    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buf.data() + cur_len, total_len - cur_len);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        cur_len += received;
    }

    // Parse application/x-www-form-urlencoded
    std::string staSsid, staPass, apSsid, apPass;
    if (extractQueryParam(buf.data(), "sta_ssid", staSsid)) {
        extractQueryParam(buf.data(), "sta_pass", staPass);
        
        // Don't save empty STA SSIDs as valid changes, unless user meant to clear it.
        // For our repeater, setting an STA SSID is the trigger.
        if (!staSsid.empty()) {
            ConfigManager::getInstance().setStaConfig(staSsid, staPass);
        }
    }

    if (extractQueryParam(buf.data(), "ap_ssid", apSsid)) {
        extractQueryParam(buf.data(), "ap_pass", apPass);
        if (!apSsid.empty()) {
            ConfigManager::getInstance().setApConfig(apSsid, apPass);
        }
    }

    ESP_LOGI(TAG, "Configuration applied. Restarting ESP32 in 2 seconds...");

    // Send Success HTML
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SUCCESS_HTML, HTTPD_RESP_USE_STRLEN);

    // Give time for HTTP response to complete before restart
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool WebServer::extractQueryParam(const char* query, const char* key, std::string& outValue) {
    char value[128] = {0};
    esp_err_t err = httpd_query_key_value(query, key, value, sizeof(value));
    if (err == ESP_OK) {
        outValue = value;
        urlDecode(outValue);
        return true;
    }
    return false;
}
