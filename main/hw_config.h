#pragma once

#include <config_state_gpio.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#define HW_CONFIG_NVS_NAME "hw_config"

struct hw_config
{
    gpio_num_t status_led_pin = GPIO_NUM_0;
    bool status_led_on_state = false;

    static std::unique_ptr<config_state_set<hw_config>> state()
    {
        auto *state = new config_state_set<hw_config>();
        state->add_field(&hw_config::status_led_pin, "/led/pin");
        state->add_field(&hw_config::status_led_on_state, "/led/pin");

        return std::unique_ptr<config_state_set<hw_config>>(state);
    }

    static const std::unique_ptr<const config_state_set<hw_config>> STATE;

    esp_err_t load();
};
