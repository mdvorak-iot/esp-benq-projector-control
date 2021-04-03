#include "device_state.h"
#include <aws_iot_mqtt_config.h>
#include <aws_iot_shadow.h>
#include <aws_iot_shadow_mqtt_error.h>
#include <esp_log.h>
#include <mqtt_client.h>

static const char TAG[] = "setup_aws_iot";

static void mqtt_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base,
                               __unused int32_t event_id, void *event_data)
{
    const esp_mqtt_event_t *event = (const esp_mqtt_event_t *)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "connecting to mqtt...");
        break;

    case MQTT_EVENT_ERROR:
        aws_iot_shadow_log_mqtt_error(TAG, event->error_handle);
        break;
    default:
        break;
    }
}

static void wifi_connected_handler(__unused void *handler_args, __unused esp_event_base_t event_base,
                                   __unused int32_t event_id, __unused void *event_data)
{
    esp_mqtt_client_handle_t mqtt_client = (esp_mqtt_client_handle_t)handler_args;

    if (xEventGroupGetBits(device_state) & STATE_BIT_MQTT_STARTED)
    {
        ESP_LOGI(TAG, "reconnect mqtt client");
        esp_err_t err = esp_mqtt_client_reconnect(mqtt_client);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "esp_mqtt_client_reconnect failed: %d (%s)", err, esp_err_to_name(err));
        }
    }
    else
    {
        // Initial connection
        ESP_LOGI(TAG, "start mqtt client");
        esp_err_t err = esp_mqtt_client_start(mqtt_client);
        if (err == ESP_OK)
        {
            xEventGroupSetBits(device_state, STATE_BIT_MQTT_STARTED);
        }
        else
        {
            ESP_LOGE(TAG, "esp_mqtt_client_start failed: %d (%s)", err, esp_err_to_name(err));
        }
    }
}

static void disconnected_handler(__unused void *handler_args, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    xEventGroupClearBits(device_state, STATE_BIT_SHADOW_READY);
}

static void shadow_ready_handler(__unused void *handler_args, __unused esp_event_base_t event_base,
                                 __unused int32_t event_id, __unused void *event_data)
{
    xEventGroupSetBits(device_state, STATE_BIT_SHADOW_READY);
}

void setup_aws_iot(esp_mqtt_client_handle_t *out_mqtt_client, aws_iot_shadow_handle_ptr *out_shadow_client)
{
    // MQTT
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.cert_pem = AWS_IOT_ROOT_CA;
    mqtt_cfg.cert_len = AWS_IOT_ROOT_CA_LEN;

    esp_err_t err = aws_iot_mqtt_config_load(&mqtt_cfg);
    if (err != ESP_OK)
    {
        // No point in continuing and logging warnings
        ESP_LOGE(TAG, "failed to load mqtt config, cannot connect to aws: %d %s", err, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "mqtt host=%s, port=%u, client_id=%s", mqtt_cfg.host ? mqtt_cfg.host : "", mqtt_cfg.port, mqtt_cfg.client_id ? mqtt_cfg.client_id : "");

    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "failed to init mqtt client");
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, mqtt_client));

    // Connect when WiFi connects
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_connected_handler, mqtt_client, NULL));

    // Shadow
    aws_iot_shadow_handle_ptr shadow_client = NULL;
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_init(mqtt_client, aws_iot_shadow_thing_name(mqtt_cfg.client_id), NULL, &shadow_client));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_DISCONNECTED, disconnected_handler, shadow_client));
    ESP_ERROR_CHECK_WITHOUT_ABORT(aws_iot_shadow_handler_register(shadow_client, AWS_IOT_SHADOW_EVENT_READY, shadow_ready_handler, shadow_client));

    // Result
    *out_mqtt_client = mqtt_client;
    *out_shadow_client = shadow_client;
}
