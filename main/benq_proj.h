#pragma once

#include <esp_err.h>
#include <hal/uart_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BENQ_PROJ_CMD_POWER_ON "pow=on"
#define BENQ_PROJ_CMD_POWER_OFF "pow=off"
#define BENQ_PROJ_CMD_BLANK_ON "blank=on"
#define BENQ_PROJ_CMD_BLANK_OFF "blank=off"
#define BENQ_PROJ_CMD_SOURCE(src) "sour=" src

#define BENQ_PROJ_SOURCE_HDMI "hdmi"
#define BENQ_PROJ_SOURCE_HDMI2 "hdmi2"
#define BENQ_PROJ_SOURCE_RGB "rgb"

struct benq_proj_config
{
    uart_port_t uart_port;
    int baud_rate;
    int rx_pin;
    int tx_pin;
};

esp_err_t benq_proj_init(const struct benq_proj_config *cfg);

esp_err_t benq_proj_command(uart_port_t uart_port, const char *command);

#ifdef __cplusplus
}
#endif
