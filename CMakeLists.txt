cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(double_reset https://github.com/mdvorak-iot/esp-double-reset.git e60f575)
ExternalComponent_Get(status_led https://github.com/mdvorak-iot/esp-status-led.git 66e4990)
#ExternalComponent_Get(wps_config https://github.com/mdvorak-iot/esp-wps-config.git 56f3061)
#ExternalComponent_Get(wifi_reconnect https://github.com/mdvorak-iot/esp-wifi-reconnect.git 56f3061)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-app-template)
