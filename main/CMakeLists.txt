cmake_minimum_required(VERSION 3.15.0)

idf_component_register(
        SRC_DIRS .
        INCLUDE_DIRS .
        REQUIRES
        freertos
        nvs_flash
        rmaker_common
        wifi_provisioning
        double_reset
        status_led
        wifi_reconnect
        app_wifi
        rainmaker_init
)
