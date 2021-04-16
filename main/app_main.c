#include "app_status.h"
#include "benq_proj.h"
#include <app_rainmaker.h>
#include <app_wifi.h>
#include <double_reset.h>
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
#define APP_PARAM_SOURCE "Source"
#define APP_PARAM_STATUS "Status"

static const char TAG[] = "app_main";

static esp_rmaker_param_t *power_param = NULL;
static esp_rmaker_param_t *blank_param = NULL;
static esp_rmaker_param_t *source_param = NULL;
static esp_rmaker_param_t *status_param = NULL;

static bool power_value = false;

// Program
static void app_devices_init(esp_rmaker_node_t *node);
static void connected_handler(__unused void *handler_arg, __unused esp_event_base_t event_base,
                              __unused int32_t event_id, __unused void *event_data);
static void proj_output_handler(const char *data, size_t len);

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
        .uart_port = CONFIG_APP_PROJ_UART_NUM,
        .baud_rate = CONFIG_APP_PROJ_UART_BAUD,
        .tx_pin = CONFIG_APP_PROJ_UART_TX_PIN,
        .rx_pin = CONFIG_APP_PROJ_UART_RX_PIN,
        .output_cb = proj_output_handler,
        .us_stack_depth = 2000,
    };
    ESP_ERROR_CHECK(benq_proj_init(&proj_cfg));

    // Start
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, node_name)); // NOTE this isn't available before WiFi init
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
    // When connection is re-established, always assume projector is powered off
    // This is simplification, because while it is possible to read the current state,
    // it is too little complex for this simple use-case
    power_value = false;
    esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(false));
    esp_rmaker_param_update_and_report(blank_param, esp_rmaker_bool(false));
}

static esp_err_t device_write_cb(__unused const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                 const esp_rmaker_param_val_t val, __unused void *private_data,
                                 __unused esp_rmaker_write_ctx_t *ctx)
{
    char *param_name = esp_rmaker_param_get_name(param);

    // Power
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0)
    {
        // Handle
        esp_err_t err = benq_proj_command(CONFIG_APP_PROJ_UART_NUM, val.val.b ? BENQ_PROJ_CMD_POWER_ON : BENQ_PROJ_CMD_POWER_OFF);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to send command: %d %s", err, esp_err_to_name(err));
            return err;
        }
        power_value = val.val.b;

        // Blank is possible only when powered on
        if (!val.val.b)
        {
            esp_rmaker_param_update_and_report(blank_param, esp_rmaker_bool(false));
        }

        // Report
        return esp_rmaker_param_update_and_report(param, val);
    }
    // Blank
    if (strcmp(param_name, APP_PARAM_BLANK) == 0)
    {
        if (power_value)
        {
            // Handle
            esp_err_t err = benq_proj_command(CONFIG_APP_PROJ_UART_NUM, val.val.b ? BENQ_PROJ_CMD_BLANK_ON : BENQ_PROJ_CMD_BLANK_OFF);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to send command: %d %s", err, esp_err_to_name(err));
                return err;
            }
            return esp_rmaker_param_update_and_report(param, val);
        }
        else
        {
            // Cannot enable
            return ESP_ERR_INVALID_STATE;
        }
    }
    // Source
    if (strcmp(param_name, APP_PARAM_SOURCE) == 0)
    {
        // Handle
        char cmd[100];
        snprintf(cmd, sizeof(cmd), BENQ_PROJ_CMD_SOURCE("%s"), val.val.s);
        esp_err_t err = benq_proj_command(CONFIG_APP_PROJ_UART_NUM, cmd);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to send command: %d %s", err, esp_err_to_name(err));
            return err;
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
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, APP_DEVICE_NAME)));
    ESP_ERROR_CHECK(esp_rmaker_node_add_device(node, device));

    // Register buttons, sensors, etc
    power_param = esp_rmaker_param_create(ESP_RMAKER_DEF_POWER_NAME, ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(power_param, ESP_RMAKER_UI_TOGGLE));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, power_param));
    ESP_ERROR_CHECK(esp_rmaker_device_assign_primary_param(device, power_param));

    blank_param = esp_rmaker_param_create(APP_PARAM_BLANK, ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(blank_param, ESP_RMAKER_UI_TOGGLE));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, blank_param));

    source_param = esp_rmaker_param_create(APP_PARAM_SOURCE, NULL, esp_rmaker_str(BENQ_PROJ_SOURCE_HDMI), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    ESP_ERROR_CHECK(esp_rmaker_param_add_ui_type(source_param, ESP_RMAKER_UI_DROPDOWN));
    static const char *sources[] = {BENQ_PROJ_SOURCE_HDMI, BENQ_PROJ_SOURCE_HDMI2, BENQ_PROJ_SOURCE_RGB};
    ESP_ERROR_CHECK(esp_rmaker_param_add_valid_str_list(source_param, sources, 3));
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, source_param));

    status_param = esp_rmaker_param_create(APP_PARAM_STATUS, NULL, esp_rmaker_str("Unknown"), PROP_FLAG_READ);
    ESP_ERROR_CHECK(esp_rmaker_device_add_param(device, status_param));
}

static void proj_output_handler(const char *data, __unused size_t len)
{
    esp_rmaker_param_update_and_report(status_param, esp_rmaker_str(data));
}
