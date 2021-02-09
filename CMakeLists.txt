cmake_minimum_required(VERSION 3.16.0)
include(ExternalComponent.cmake)

# Components
ExternalComponent_Get(esp-double-reset https://github.com/mdvorak-iot/esp-double-reset.git 70b4c2b)
#ExternalComponent_Get(esp-wps-config https://github.com/mdvorak-iot/esp-wps-config.git 56f3061)

# Project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-app-template)
