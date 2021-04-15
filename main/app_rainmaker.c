#include "app_rainmaker.h"
#include <app_wifi.h>
#include <app_wifi_defs.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_schedule.h>
#include <esp_wifi.h>

static const char TAG[] = "app_rainmaker";

static void print_qrcode_handler(__unused void *arg, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    char payload[200];
    // {"ver":"%s","name":"%s","pop":"%s","transport":"%s"}
    snprintf(payload, sizeof(payload), "%%7B%%22ver%%22%%3A%%22%s%%22%%2C%%22name%%22%%3A%%22%s%%22%%2C%%22pop%%22%%3A%%22%s%%22%%2C%%22transport%%22%%3A%%22%s%%22%%7D",
             "v1", app_wifi_prov_get_service_name(), app_wifi_get_prov_pop(), APP_WIFI_PROV_TRANSPORT);
    ESP_LOGI(TAG, "To view QR Code, copy paste the URL in a browser:\n%s?data=%s", "https://rainmaker.espressif.com/qrcode.html", payload);
}

esp_err_t app_rmaker_node_name(char *node_name, size_t node_name_len)
{
    // App info
    esp_app_desc_t app_info = {};
    esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info);

    // Construct node name
    uint8_t eth_mac[6] = {};
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

    snprintf(node_name, node_name_len, "%s-%02x%02x", app_info.project_name, eth_mac[0], eth_mac[1]);
    ESP_LOGI(TAG, "using node name '%s'", node_name);
    return ESP_OK;
}

esp_err_t app_rmaker_init(const char *node_name, esp_rmaker_node_t **out_node)
{
    esp_err_t err = ESP_OK;

    // App info
    esp_app_desc_t app_info = {};
    esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info);

    // Create node
    esp_rmaker_config_t cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&cfg, node_name, app_info.project_name);
    if (!node) goto error;

    // Print QR on provisioning
    esp_event_handler_instance_t qrcode_handler_instance = NULL;
    err = esp_event_handler_instance_register(WIFI_PROV_EVENT, WIFI_PROV_START, print_qrcode_handler, node, &qrcode_handler_instance);
    if (err != ESP_OK) goto error;

    // Initialize
    err = esp_rmaker_schedule_enable();
    if (err != ESP_OK) goto error;

    esp_rmaker_ota_config_t ota_config = {
        .server_cert = (char *)ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
    };
    err = esp_rmaker_ota_enable(&ota_config, OTA_USING_TOPICS);
    if (err != ESP_OK) goto error;

    // Return
    *out_node = node;
    return ESP_OK;

error:
    if (node) esp_rmaker_node_deinit(node);
    if (qrcode_handler_instance) esp_event_handler_instance_unregister(WIFI_PROV_EVENT, WIFI_PROV_START, qrcode_handler_instance);
    return err;
}
