#pragma once

#include <esp_err.h>
#include <hal/uart_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct benq_proj_config
{
    uart_port_t uart_port;
    int baud_rate;
    int rx_pin;
    int tx_pin;
};

esp_err_t benq_proj_init(const struct benq_proj_config *cfg);

#ifdef __cplusplus
}
#endif
