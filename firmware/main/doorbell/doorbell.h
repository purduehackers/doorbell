#ifndef DOORBELL_H
#define DOORBELL_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define DOORBELL_PIN 3

extern EventGroupHandle_t doorbell_events;
#define DOORBELL_PRESSED            BIT0
#define DOORBELL_FINISHED_RINGING   BIT1

void start_doorbell();

void prepare_doorbell_for_sleep();
void wake_doorbell_from_sleep(bool triggered);

#endif
