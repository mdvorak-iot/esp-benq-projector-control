idf_component_register(
    SRC_DIRS .
    INCLUDE_DIRS .
    REQUIRES 
        freertos
        nvs_flash
        log
        app_update
        esp32
        esp_wifi
        double_reset 
        status_led
        #wps_config
        #wifi_reconnect
)
