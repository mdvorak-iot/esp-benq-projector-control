#include "app_status.h"
#include <app_wifi.h>
#include <app_wifi_defs.h>
#include <double_reset.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>
#include <wifi_reconnect.h>

static const char TAG[] = "app_main";

#define RAINMAKER_NODE_TYPE "Template"

// Global state
static void app_services_init(esp_rmaker_node_t *node, const char *default_name);
static void print_qrcode_handler(__unused void *arg, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data);
static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx);

void app_main()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // System services
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Check double reset
    // NOTE this should be called as soon as possible, ideally right after nvs init
    bool reconfigure = false;
    ESP_ERROR_CHECK_WITHOUT_ABORT(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));

    // App info
    esp_app_desc_t app_info = {};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info));

    // Setup
    app_status_init();

    struct app_wifi_config wifi_cfg = {
        .security = WIFI_PROV_SECURITY_1,
        .service_name = app_info.project_name,
        .hostname = app_info.project_name,
        .wifi_connect = wifi_reconnect_resume,
    };
    ESP_ERROR_CHECK(app_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(wifi_reconnect_start());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT, WIFI_PROV_START, print_qrcode_handler, NULL, NULL));

    // RainMaker
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, app_info.project_name, RAINMAKER_NODE_TYPE);
    if (!node)
    {
        ESP_LOGE(TAG, "could not initialize node, aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    app_services_init(node, app_info.project_name);

    // Enable OTA
    esp_rmaker_ota_config_t ota_config = {
        .server_cert = (char *)ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
    };
    ESP_ERROR_CHECK(esp_rmaker_ota_enable(&ota_config, OTA_USING_TOPICS));

    // Start
    ESP_ERROR_CHECK(esp_rmaker_schedule_enable());
    ESP_ERROR_CHECK(esp_rmaker_start());
    ESP_ERROR_CHECK(app_wifi_start(reconfigure));

    // Run
    ESP_LOGI(TAG, "life is good");
}

static void print_qrcode_handler(__unused void *arg, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    const char version[] = "v1";
#if APP_WIFI_PROV_TYPE_BLE
    const char transport[] = "ble";
#elif APP_WIFI_PROV_TYPE_SOFT_AP
    const char transport[] = "softap";
#endif

    char payload[200];
    // {"ver":"%s","name":"%s","pop":"%s","transport":"%s"}
    snprintf(payload, sizeof(payload), "%%7B%%22ver%%22%%3A%%22%s%%22%%2C%%22name%%22%%3A%%22%s%%22%%2C%%22pop%%22%%3A%%22%s%%22%%2C%%22transport%%22%%3A%%22%s%%22%%7D",
             version, app_wifi_prov_get_service_name(), app_wifi_get_prov_pop(), transport);
    ESP_LOGI(TAG, "To view QR Code, copy paste the URL in a browser:\n%s?data=%s", "https://rainmaker.espressif.com/qrcode.html", payload);
}

static void app_services_init(esp_rmaker_node_t *node, const char *default_name)
{
    // Prepare device
    esp_rmaker_device_t *device = esp_rmaker_device_create("Fan", ESP_RMAKER_DEVICE_FAN, NULL);
    assert(device);

    esp_rmaker_device_add_cb(device, device_write_cb, NULL);
    esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, default_name));
    esp_rmaker_node_add_device(node, device);

    // Register buttons, sensors, etc
}

static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx)
{
    //    char *param_name = esp_rmaker_param_get_name(param);
    //    if (strcmp(param_name, "TODO") == 0)
    //    {
    //        // TODO handle
    //        esp_rmaker_param_update_and_report(param, val);
    //    }
    return ESP_OK;
}
