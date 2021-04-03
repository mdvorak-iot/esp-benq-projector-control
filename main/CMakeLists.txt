cmake_minimum_required(VERSION 3.15.0)

idf_component_register(
        SRC_DIRS .
        INCLUDE_DIRS .
        REQUIRES
        freertos
        nvs_flash
        log
        esp32
        esp_wifi
        app_update
        double_reset
        status_led
        wps_config
        wifi_reconnect
        rapidjson
        config_state
        aws_iot_mqtt_config
        aws_iot_shadow
)
