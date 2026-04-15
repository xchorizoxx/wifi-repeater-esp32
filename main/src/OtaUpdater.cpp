#include "OtaUpdater.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>

// Define max buffer for receiving OTA chunks
#define OTA_BUF_SIZE 1024

esp_err_t OtaUpdater::postUpdateHandler(httpd_req_t* req) {
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t* update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA update...");

    const esp_partition_t* configured = esp_ota_get_boot_partition();
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08" PRIx32 ", but running from offset 0x%08" PRIx32,
                 configured->address, running->address);
        ESP_LOGW(TAG, "This can happen if either the OTA boot data or preferred boot image become corrupted somehow.");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08" PRIx32 ")",
             running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Next update partition not found! Check partition table!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition not found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32,
             update_partition->subtype, update_partition->address);

    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        esp_ota_abort(update_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed");
        return err;
    }

    int remaining = req->content_len;
    char ota_write_data[OTA_BUF_SIZE + 1] = {0};

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, ota_write_data, std::min(remaining, OTA_BUF_SIZE));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Retry receiving
            }
            ESP_LOGE(TAG, "OTA RX failed!");
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection closed");
            return ESP_FAIL;
        }

        err = esp_ota_write(update_handle, (const void*)ota_write_data, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_write failed");
            return err;
        }
        remaining -= recv_len;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return err;
    }

    ESP_LOGI(TAG, "OTA Update complete. Restarting in 2 seconds...");

    // Send Success
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OTA Success. Rebooting...", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}
