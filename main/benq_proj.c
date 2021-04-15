#include "benq_proj.h"
#include <driver/uart.h>
#include <esp_log.h>
#include <string.h>

static const char TAG[] = "benq_proj";

#define BENQ_PROJ_UART_BUFFER_SIZE 256
#define BENQ_PROJ_UART_QUEUE_SIZE 16

struct benq_proj_context
{
    uart_port_t uart_port;
    QueueHandle_t event_queue;
};

_Noreturn static void benq_proj_task(void *arg)
{
    struct benq_proj_context *ctx = (struct benq_proj_context *)arg;
    uart_event_t event = {};

    while (1)
    {
        if (xQueueReceive(ctx->event_queue, &event, pdMS_TO_TICKS(200)))
        {
            switch (event.type)
            {
            case UART_DATA:
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                uart_flush(ctx->uart_port);
                xQueueReset(ctx->event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                uart_flush(ctx->uart_port);
                xQueueReset(ctx->event_queue);
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "Rx Break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "Frame Error");
                break;
            case UART_PATTERN_DET:
                // TODO esp_handle_uart_pattern(ctx);
                break;
            default:
                ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                break;
            }
        }
        /* Drive the event loop */
        // TODO esp_event_loop_run(ctx->event_loop_hdl, pdMS_TO_TICKS(50));
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

    // TODO handle error and return
    ESP_ERROR_CHECK(uart_param_config(cfg->uart_port, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(cfg->uart_port, cfg->tx_pin, cfg->rx_pin, -1, -1));

    QueueHandle_t event_queue = NULL;
    ESP_ERROR_CHECK(uart_driver_install(cfg->uart_port, BENQ_PROJ_UART_BUFFER_SIZE, BENQ_PROJ_UART_BUFFER_SIZE, BENQ_PROJ_UART_QUEUE_SIZE, &event_queue, 0));

    // TODO malloc
    struct benq_proj_context *ctx = malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    ctx->uart_port = cfg->uart_port;
    ctx->event_queue = event_queue;

    // TODO
    xTaskCreate(benq_proj_task, "benq_proj", 1024, ctx, 1, NULL);

    return ESP_OK;
}
