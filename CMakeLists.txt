cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(double_reset https://github.com/mdvorak-iot/esp-double-reset.git 4ab520a)
ExternalComponent_Get(status_led https://github.com/mdvorak-iot/esp-status-led.git c9e5947)
ExternalComponent_Get(wps_config https://github.com/mdvorak-iot/esp-wps-config.git 44f75bb)
ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git 9d91a95)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-app-template)
