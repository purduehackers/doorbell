#ifndef PATTERN_DRIVER_THREAD_H
#define PATTERN_DRIVER_THREAD_H

#include "freertos/idf_additions.h"

enum CurrentPattern {
    CurrentPattern_StartingUp = -1,
    CurrentPattern_Off = 0,
    CurrentPattern_Error = 1,
    CurrentPattern_WifiDisconnected = 2,
    CurrentPattern_Updating = 3,
    CurrentPattern_Ringing_Ringing = 4,
    CurrentPattern_Ringing_Sending = 5,
};

extern enum CurrentPattern current_pattern;
extern int current_pattern_data;

extern SemaphoreHandle_t current_pattern_semaphore;
extern EventGroupHandle_t current_pattern_events;
#define CURRENT_PATTERN_UPDATED     BIT0
#define CURRENT_PATTERN_COMPLETE    BIT1

void led_pattern_driver_thread_entrypoint(void * arg);

#endif
