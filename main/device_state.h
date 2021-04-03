#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATE_BIT_RECONFIGURE BIT0
#define STATE_BIT_MQTT_STARTED BIT1
#define STATE_BIT_SHADOW_READY BIT2

extern EventGroupHandle_t device_state;

void device_state_init();

#ifdef __cplusplus
}
#endif
