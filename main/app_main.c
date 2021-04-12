#include "app_wifi.h"
#include <double_reset.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_rmaker_core.h>
#include <nvs_flash.h>

static const char TAG[] = "app_main";

static const char RAINMAKER_DEVICE_TYPE[] = "esp.device.custom";

// Global state
void app_services_init(esp_rmaker_node_t *node);

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
    // void app_status_init(status_led_pin, status_led_on_state); // TODO
    app_wifi_init(app_info.project_name);

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, app_info.project_name, RAINMAKER_DEVICE_TYPE);
    if (!node)
    {
        ESP_LOGE(TAG, "could not initialize node, aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    app_services_init(node);

    // Start
    esp_rmaker_start();
    app_wifi_start(reconfigure);

    // Run
    ESP_LOGI(TAG, "life is good");
}

void app_services_init(esp_rmaker_node_t *node)
{
    // Register buttons, sensors, etc
}
