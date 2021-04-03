#include "device_state.h"
#include "hw_config.h"
#include <double_reset.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <status_led.h>
#include <wps_config.h>

static const char TAG[] = "main";
static const auto STATUS_LED_CONNECTING_INTERVAL = 500u;
static const auto STATUS_LED_WPS_INTERVAL = 100u;

// Global state
static status_led_handle_ptr status_led;
static hw_config config = {};

static void setup_init();
static void setup_devices();
extern "C" void setup_wifi(const char *hostname);
static void setup_final(const esp_app_desc_t *app_info);

extern "C" void app_main()
{
    esp_app_desc_t app_info = {};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_info));

    // Setup
    setup_init();
    setup_devices();
    setup_wifi(app_info.project_name);
    setup_final(&app_info);

    // Run
    ESP_LOGI(TAG, "life is good");
}

// Setup logic
static void status_led_wps_event_handler(__unused void *arg, esp_event_base_t event_base,
                                         int32_t event_id, __unused void *event_data)
{
    if (event_base == WIFI_EVENT
        && (event_id == WIFI_EVENT_STA_WPS_ER_SUCCESS || event_id == WIFI_EVENT_STA_WPS_ER_TIMEOUT || event_id == WIFI_EVENT_STA_WPS_ER_FAILED))
    {
        status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true);
    }
    else if (event_base == WPS_CONFIG_EVENT && event_id == WPS_CONFIG_EVENT_START)
    {
        status_led_set_interval(status_led, STATUS_LED_WPS_INTERVAL, true);
    }
}

static void status_led_disconnected_handler(__unused void *arg, esp_event_base_t event_base,
                                            __unused int32_t event_id, __unused void *event_data)
{
    status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true);
}

static void setup_init()
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
    device_state_init();

    // Check double reset
    // NOTE this should be called as soon as possible, ideally right after nvs init
    bool reconfigure = false;
    ESP_ERROR_CHECK_WITHOUT_ABORT(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));
    if (reconfigure)
    {
        xEventGroupSetBits(device_state, STATE_BIT_RECONFIGURE);
    }

    // Load config
    err = config.load();
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "failed to load config: %d %s", err, esp_err_to_name(err));
    }

    // Status LED
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_create(config.status_led_pin, config.status_led_on_state, &status_led));
    ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_set_interval(status_led, STATUS_LED_CONNECTING_INTERVAL, true));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, status_led_wps_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WPS_CONFIG_EVENT, WPS_CONFIG_EVENT_START, status_led_wps_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, status_led_disconnected_handler, nullptr, nullptr));
}

static void setup_devices()
{
    // Custom devices and other init, that needs to be done before waiting for wifi connection
}

static void setup_final(const esp_app_desc_t *app_info)
{
    // Ready
    ESP_LOGI(TAG, "started %s %s", app_info->project_name, app_info->version);
}
