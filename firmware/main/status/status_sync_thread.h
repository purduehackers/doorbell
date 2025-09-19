#ifndef STATUS_SYNC_THREAD_H
#define STATUS_SYNC_THREAD_H

#include <stdbool.h>

#include "freertos/idf_additions.h"

extern bool system_ready;

extern int indicating_error;
extern enum WifiStatus indicating_wifi_status;
extern bool indicating_updating;
extern enum RingingStatus indicating_ringing;

extern SemaphoreHandle_t status_state_semaphore;
extern EventGroupHandle_t status_state_events;
#define STATUS_STATE_UPDATED    BIT0

void led_status_sync_thread_entrypoint(void * arg);

#endif
