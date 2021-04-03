#include "device_state.h"

EventGroupHandle_t device_state = NULL;

void device_state_init()
{
    assert(!device_state);
    device_state = xEventGroupCreate();
    assert(device_state);
}
