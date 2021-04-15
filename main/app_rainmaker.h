#pragma once

#include <esp_rmaker_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_RMAKER_NODE_NAME_LEN (32 + 5)

esp_err_t app_rmaker_node_name(char *node_name, size_t node_name_len);

esp_err_t app_rmaker_init(const char *node_name, esp_rmaker_node_t **out_node);

#ifdef __cplusplus
}
#endif
