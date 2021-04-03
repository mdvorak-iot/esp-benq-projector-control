#include "hw_config.h"

const std::unique_ptr<const config_state_set<hw_config>> hw_config::STATE = hw_config::state();

esp_err_t hw_config::load()
{
    esp_err_t err = ESP_FAIL;
    auto handle = nvs::open_nvs_handle(HW_CONFIG_NVS_NAME, NVS_READONLY, &err);

    if (err == ESP_OK)
    {
        err = hw_config::STATE->load(*this, handle);
    }

    return err;
}
