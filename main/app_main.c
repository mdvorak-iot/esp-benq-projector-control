#include "app_rainmaker.h"
#include "app_status.h"
#include "benq_proj.h"
#include <app_wifi.h>
#include <double_reset.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>
#include <wifi_reconnect.h>

#define APP_DEVICE_NAME CONFIG_APP_DEVICE_NAME
#define APP_DEVICE_TYPE CONFIG_APP_DEVICE_TYPE
#define APP_PARAM_BLANK "Blank"

static const char TAG[] = "app_main";

static esp_rmaker_param_t *power_param = NULL;
static esp_rmaker_param_t *blank_param = NULL;

// Program
static void app_devices_init(esp_rmaker_node_t *node);
static void connected_handler(__unused void *handler_arg, __unused esp_event_base_t event_base,
                              __unused int32_t event_id, __unused void *event_data);

void setup()
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

    // Setup
    app_status_init();

    struct app_wifi_config wifi_cfg = {
        .security = WIFI_PROV_SECURITY_1,
        .wifi_connect = wifi_reconnect_resume,
    };
    ESP_ERROR_CHECK(app_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(wifi_reconnect_start());

    // RainMaker
    char node_name[APP_RMAKER_NODE_NAME_LEN] = {};
    ESP_ERROR_CHECK(app_rmaker_node_name(node_name, sizeof(node_name)));

    esp_rmaker_node_t *node = NULL;
    ESP_ERROR_CHECK(app_rmaker_init(node_name, &node));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED, connected_handler, node, NULL));

    app_devices_init(node);

    // Configure UART
    struct benq_proj_config proj_cfg = {
        .uart_port = UART_NUM_2,
        .baud_rate = CONFIG_APP_PROJ_UART_BAUD,
        .tx_pin = CONFIG_APP_PROJ_UART_TX_PIN,
        .rx_pin = CONFIG_APP_PROJ_UART_RX_PIN,
    };
    ESP_ERROR_CHECK(benq_proj_init(&proj_cfg));

    // Start
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, node_name)); // NOTE this wasn't available before during WiFi init
    ESP_ERROR_CHECK(esp_rmaker_start());
    ESP_ERROR_CHECK(app_wifi_start(reconfigure));

    // Done
    ESP_LOGI(TAG, "setup complete");
}

void app_main()
{
    setup();

    // Run
    ESP_LOGI(TAG, "life is good");
}

static void connected_handler(__unused void *handler_arg, __unused esp_event_base_t event_base,
                              __unused int32_t event_id, __unused void *event_data)
{
    // Report state
    bool power = false; // TODO read
    esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(power));

    esp_rmaker_param_update_and_report(blank_param, esp_rmaker_bool(false)); // TODO
}

static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx)
{
    char *param_name = esp_rmaker_param_get_name(param);

    // Power
    if (strcmp(param_name, ESP_RMAKER_PARAM_POWER) == 0)
    {
        // TODO handle

        // Blank is possible only when powered on
        if (!val.val.b)
        {
            esp_rmaker_param_update_and_report(blank_param, esp_rmaker_bool(false));
        }

        // Report
        return esp_rmaker_param_update_and_report(param, val);
    }

    return ESP_OK;
}

static void app_devices_init(esp_rmaker_node_t *node)
{
    // Prepare device
    esp_rmaker_device_t *device = esp_rmaker_device_create(APP_DEVICE_NAME, APP_DEVICE_TYPE, NULL);
    assert(device);

    ESP_ERROR_CHECK(esp_rmaker_device_add_cb(device, device_write_cb, NULL));

    char *node_name = esp_rmaker_node_get_info(node)->name;
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, node_name)));
    ESP_ERROR_CHECK(esp_rmaker_node_add_device(node, device));

    // Register buttons, sensors, etc
    power_param = esp_rmaker_param_create(ESP_RMAKER_DEF_POWER_NAME, ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(power_param, ESP_RMAKER_UI_TOGGLE);
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, power_param));

    blank_param = esp_rmaker_param_create(APP_PARAM_BLANK, ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(blank_param, ESP_RMAKER_UI_TOGGLE);
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, blank_param));
}
