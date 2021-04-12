#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_wifi_init(const char *hostname);

void app_wifi_start(bool reconfigure);

#ifdef __cplusplus
}
#endif
