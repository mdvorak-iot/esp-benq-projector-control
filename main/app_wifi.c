#include <esp_log.h>
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>
#include <wifi_reconnect.h>

#define APP_WIFI_PROV_TYPE_BLE CONFIG_APP_WIFI_PROV_TYPE_BLE
#define APP_WIFI_PROV_TYPE_SOFT_AP CONFIG_APP_WIFI_PROV_TYPE_SOFT_AP
#define APP_WIFI_PROV_TIMEOUT_S CONFIG_APP_WIFI_PROV_TIMEOUT_S

#if APP_WIFI_PROV_TYPE_BLE
#include <wifi_provisioning/scheme_ble.h>
#elif APP_WIFI_PROV_TYPE_SOFT_AP
#include <wifi_provisioning/scheme_softap.h>
#endif

static const char TAG[] = "app_wifi_init";

static esp_timer_handle_t wifi_prov_timeout_timer = NULL;
static wifi_config_t startup_wifi_config = {};

static void wifi_prov_timeout_timer_delete()
{
    if (wifi_prov_timeout_timer)
    {
        esp_timer_stop(wifi_prov_timeout_timer);
        esp_timer_delete(wifi_prov_timeout_timer);
        wifi_prov_timeout_timer = NULL;
    }
}

static void wifi_prov_timeout_handler(__unused void *arg)
{
    ESP_LOGI(TAG, "provisioning timeout");
    wifi_prov_mgr_stop_provisioning();
    // Note: everything is done in WIFI_PROV_END handler
}

static void wifi_prov_event_handler(__unused void *arg, __unused esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_PROV_START:
        ESP_LOGI(TAG, "provisioning started");
        break;
    case WIFI_PROV_CRED_RECV: {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(TAG, "provisioning received ssid '%s'", (const char *)wifi_sta_cfg->ssid);
        break;
    }
    case WIFI_PROV_CRED_FAIL: {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGE(TAG, "provisioning failed: %s", (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "wifi STA authentication failed" : "wifi AP not found");
        // Note: Let the timeout kill provisioning, even if it won't connect anyway
        break;
    }
    case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "provisioning successful");
        break;
    case WIFI_PROV_END: {
        ESP_LOGI(TAG, "provisioning end");
        wifi_prov_timeout_timer_delete();
        wifi_prov_mgr_deinit();

        // When successful, config should be correctly set
        // On timeout, it needs to be reset manually (probably bug in wifi config stack)
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
        wifi_config_t current_cfg = {};
        ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &current_cfg));

        if (current_cfg.sta.ssid[0] == '\0')
        {
            ESP_LOGI(TAG, "wifi credentials not found after provisioning, trying startup wifi config");
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &startup_wifi_config));
        }
        else if (startup_wifi_config.sta.ssid[0] == '\0')
        {
            // Nothing we can do, no internet connectivity
            ESP_LOGI(TAG, "no wifi credentials found");
        }

        wifi_reconnect_resume();
        break;
    }
    default:
        break;
    }
}

static esp_err_t get_device_pop(char *pop, size_t max)
{
    if (!pop)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t eth_mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    if (err == ESP_OK)
    {
        snprintf(pop, max, "%02x%02x%02x%02x", eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
        return ESP_OK;
    }
    return err;
}

// TODO this should be outside wifi provisioning code, but it needs pop
static void app_wifi_print_qr(const char *name, const char *pop, const char *transport)
{
    if (!name || !pop || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[200];
    // {"ver":"%s","name":"%s","pop":"%s","transport":"%s"}
    snprintf(payload, sizeof(payload), "%%7B%%22ver%%22%%3A%%22%s%%22%%2C%%22name%%22%%3A%%22%s%%22%%2C%%22pop%%22%%3A%%22%s%%22%%2C%%22transport%%22%%3A%%22%s%%22%%7D",
             "v1", name, pop, transport);
    ESP_LOGI(TAG, "To view QR Code, copy paste the below URL in a browser:\n%s?data=%s", "https://rainmaker.espressif.com/qrcode.html", payload);
}

void app_wifi_init(const char *hostname)
{
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
    ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

    // Store original STA config, so it can be used on provisioning timeout
    // Note: This is needed, since wifi stack is unable to re-read correct config from NVS after provisioning for some reason
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &startup_wifi_config));

    // Listen for events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_prov_event_handler, NULL));
}

void app_wifi_start(bool reconfigure)
{
    // Initialize provisioning
    wifi_prov_mgr_config_t wifi_prof_cfg = {
#if APP_WIFI_PROV_TYPE_BLE
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
#elif APP_WIFI_PROV_TYPE_SOFT_AP
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
#endif
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(wifi_prof_cfg));

    // Detect provisioning mode
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned || reconfigure)
    {
        // Provisioning mode
        ESP_LOGI(TAG, "provisioning starting, timeout %d s", APP_WIFI_PROV_TIMEOUT_S);

        const char *hostname = NULL;
        ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &hostname));

        // Service name
        char service_name[65] = {}; // Note: only first 29 chars will be probably broadcast
        snprintf(service_name, sizeof(service_name), "PROV_%s", hostname);
        ESP_LOGI(TAG, "service name: %s", service_name);

        // Proof of possession
        // NOTE this uses atm MAC, which is stable, and QR code can be printed on the device
        char pop[9] = {};
        ESP_ERROR_CHECK_WITHOUT_ABORT(get_device_pop(pop, sizeof(pop)));

        // Start
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, pop, service_name, NULL));

        esp_timer_create_args_t args = {
            .callback = wifi_prov_timeout_handler,
            .name = "wifi_prov_timeout_timer",
        };
        ESP_ERROR_CHECK(esp_timer_create(&args, &wifi_prov_timeout_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(wifi_prov_timeout_timer, APP_WIFI_PROV_TIMEOUT_S * 1000000));

        // QR code
#if APP_WIFI_PROV_TYPE_BLE
        app_wifi_print_qr(service_name, pop, "ble");
#elif APP_WIFI_PROV_TYPE_SOFT_AP
        app_wifi_print_qr(service_name, pop, "softap");
#endif
    }
    else
    {
        // Deallocate wifi provisioning
        wifi_prov_mgr_deinit();

        // Connect to existing wifi
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_reconnect_resume();
    }
}