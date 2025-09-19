#ifndef SOCKET_H
#define SOCKET_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t websocket_events;
#define SOCKET_READY        BIT0
#define SOCKET_CONNECTED    BIT1
#define DOORBELL_RUNG       BIT2

void init_socket_state();

void start_socket();
void stop_socket();

void ring_doorbell(bool wait_for_connection);

#endif
