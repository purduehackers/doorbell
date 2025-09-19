#ifndef STATUS_H
#define STATUS_H

#include <stdbool.h>

#include "freertos/idf_additions.h"

#include "driver/ledc.h"

#define LED_LS_TIMER          LEDC_TIMER_1
#define LED_LS_MODE           LEDC_LOW_SPEED_MODE
#define LED_LS_CH2_GPIO       2
#define LED_LS_CH2_CHANNEL    LEDC_CHANNEL_2

#define LED_MAX_DUTY          8191
#define LED_MEDIUM_DUTY       2047
#define LED_INDICATE_DUTY     256

#define LED_SAFE_PWM_CYCLE_DELAY 1

#define LED_TEST_FADE_TIME    1000

#define ERROR_MAX_BITS        4
#define ERROR_HOLD_TIME       250

#define WIFI_DISCONNECTED_SHORT_TIME 350
#define WIFI_DISCONNECTED_LONG_TIME  1000
#define WIFI_DISCONNECTED_OFF_TIME   1000

#define UPDATING_FADE_TIME    1000

#define RINGING_FADE_TIME    500

enum RingingStatus {
    RingingStatus_Off = 0,
    RingingStatus_Sending = 1,
    RingingStatus_Ringing = 2,
};

enum WifiStatus {
    WifiStatus_Connecting = 0,
    WifiStatus_Connected = 1,
};

extern ledc_timer_config_t led_timer;
extern ledc_channel_config_t led_channel;
extern ledc_cbs_t led_callbacks;

extern SemaphoreHandle_t fade_complete_semaphore;

void start_status();

void ready_status();

void update_ringing_status(enum RingingStatus ringing);
void update_updating_status(bool updating);
void update_wifi_status(enum WifiStatus wifi_status);
void display_error(int error);

void prepare_status_for_sleep();
void wake_status_from_sleep();

#endif
