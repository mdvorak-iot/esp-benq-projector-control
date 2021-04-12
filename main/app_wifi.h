#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_wifi_init(const char *hostname);

void app_wifi_connect(bool reconfigure);

#ifdef __cplusplus
}
#endif

#endif
