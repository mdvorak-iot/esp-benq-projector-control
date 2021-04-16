#include "benq_proj.h"
#include <driver/uart.h>
#include <esp_log.h>
#include <string.h>

static const char TAG[] = "benq_proj";

#define BENQ_PROJ_UART_BUFFER_SIZE 256

struct benq_proj_context
{
    uart_port_t uart_port;
    benq_proj_output_cb output_cb;
};

static int trim_output(uint8_t **data, int len)
{
    // Trim initial white chars
    uint8_t *ptr = *data;
    while (ptr < *data + len)
    {
        if (*ptr > 0x20 && *ptr <= 127) break;
        ++ptr;
    }
    // Trim trailing white chars
    uint8_t *end = *data + len;
    while (end > ptr)
    {
        uint8_t c = *(end - 1);
        if (c > 0x20 && c <= 127) break;
        --end;
    }

    // Result
    *data = ptr;
    return (int)(end - ptr);
}

_Noreturn static void benq_proj_task(void *arg)
{
    struct benq_proj_context *ctx = (struct benq_proj_context *)arg;

    uint8_t *data = (uint8_t *)malloc(BENQ_PROJ_UART_BUFFER_SIZE + 1); // space for null terminator
    while (1)
    {
        int len = uart_read_bytes(ctx->uart_port, data, BENQ_PROJ_UART_BUFFER_SIZE, 100 / portTICK_RATE_MS);
        if (len > 0)
        {
            uint8_t *ptr = data;
            len = trim_output(&ptr, len);
            ptr[len] = '\0'; // we have always space in the buffer for a null terminator

            ESP_LOGI(TAG, "response: %.*s", len, (const char *)ptr);

            if (ctx->output_cb)
            {
                ctx->output_cb((const char *)ptr, len);
            }
        }
    }
}

esp_err_t benq_proj_init(const struct benq_proj_config *cfg)
{
    uart_config_t uart_cfg = {
        .baud_rate = cfg->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    esp_err_t err = ESP_OK;

    if ((err = uart_driver_install(cfg->uart_port, BENQ_PROJ_UART_BUFFER_SIZE * 2, 0, 0, NULL, intr_alloc_flags)) != ESP_OK)
    {
        return err;
    }
    if ((err = uart_param_config(cfg->uart_port, &uart_cfg)) != ESP_OK)
    {
        return err;
    }
    if ((err = uart_set_pin(cfg->uart_port, cfg->tx_pin, cfg->rx_pin, -1, -1)) != ESP_OK)
    {
        return err;
    }

    // Allocate context for the task
    struct benq_proj_context *ctx = malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    ctx->uart_port = cfg->uart_port;
    ctx->output_cb = cfg->output_cb;

    // Start background RX task
    uint32_t us_stack_depth = cfg->us_stack_depth ? cfg->us_stack_depth : 1000;
    if (xTaskCreate(benq_proj_task, "benq_proj", us_stack_depth, ctx, 1, NULL) != pdPASS)
    {
        return ESP_FAIL;
    }

    // Success
    ESP_LOGI(TAG, "initialized on %d pins tx=%d rx=%d", cfg->uart_port, cfg->tx_pin, cfg->rx_pin);
    return ESP_OK;
}

esp_err_t benq_proj_command(uart_port_t uart_port, const char *command)
{
    char cmd_str[100];
    int len = snprintf(cmd_str, sizeof(cmd_str), "\r*%s#\r", command);

    ESP_LOGI(TAG, "sending: %.*s", len - 2, cmd_str + 1); // log without \r
    return uart_write_bytes(uart_port, cmd_str, len) == len ? ESP_OK : ESP_FAIL;
}
