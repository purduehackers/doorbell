#include "pattern_driver_thread.h"

#include "status.h"

#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "status (led pattern driver thread)";

enum CurrentPattern current_pattern;
int current_pattern_data;

SemaphoreHandle_t current_pattern_semaphore;
EventGroupHandle_t current_pattern_events;

void led_pattern_driver_thread_entrypoint(void * arg)
{
    enum CurrentPattern current_pattern_internal = -1;
    int current_pattern_data_internal = 0;
    int current_pattern_progress = 0;

    ESP_LOGI(TAG, "running led fade in...");

    ledc_set_fade_with_time(
        led_channel.speed_mode,
        led_channel.channel,
        LED_INDICATE_DUTY,
        LED_TEST_FADE_TIME
    );
    ledc_fade_start(
        led_channel.speed_mode,
        led_channel.channel,
        LEDC_FADE_NO_WAIT
    );

    xSemaphoreTake(fade_complete_semaphore, portMAX_DELAY);

    ESP_LOGI(TAG, "led fade in done...");

    ESP_LOGI(TAG, "waiting for system ready pattern...");

    while (1)
    {
        if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
        {
            if (current_pattern == CurrentPattern_StartingUp)
            {
                xSemaphoreGive(current_pattern_semaphore);
                xEventGroupWaitBits(current_pattern_events, CURRENT_PATTERN_UPDATED, pdFALSE, pdFALSE, portMAX_DELAY);
            }
            else
            {
                current_pattern_internal = 0;
                xSemaphoreGive(current_pattern_semaphore);
                break;
            }
        }
        else
        {
            vTaskDelay(2 / portTICK_PERIOD_MS);
        }
    }

    ESP_LOGI(TAG, "running led fade out...");

    ledc_set_fade_with_time(
        led_channel.speed_mode,
        led_channel.channel,
        0,
        LED_TEST_FADE_TIME
    );
    ledc_fade_start(
        led_channel.speed_mode,
        led_channel.channel,
        LEDC_FADE_NO_WAIT
    );

    xSemaphoreTake(fade_complete_semaphore, portMAX_DELAY);

    while (1)
    {
        if (xSemaphoreTake(fade_complete_semaphore, 0))
        {
            ESP_LOGI(TAG, "fade complete event triggered");

            if (current_pattern_internal == CurrentPattern_WifiDisconnected)
            {
                ESP_LOGI(TAG, "next step in the wifi pattern: %d", current_pattern_progress);

                if (current_pattern_progress == 0)
                {
                    ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                    ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                    vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

                    ledc_set_fade_with_time(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LED_MEDIUM_DUTY,
                        WIFI_DISCONNECTED_SHORT_TIME
                    );
                    ledc_fade_start(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LEDC_FADE_NO_WAIT
                    );
                }
                else
                {
                    ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                    ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                    vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

                    ledc_set_fade_with_time(
                        led_channel.speed_mode,
                        led_channel.channel,
                        0,
                        WIFI_DISCONNECTED_LONG_TIME
                    );
                    ledc_fade_start(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LEDC_FADE_NO_WAIT
                    );
                }

                current_pattern_progress = (current_pattern_progress + 1) % 2;
            }
            else if (current_pattern_internal == CurrentPattern_Updating)
            {
                current_pattern_progress = 0;

                ESP_LOGI(TAG, "flashing update pattern");

                ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

                ledc_set_fade_with_time(
                    led_channel.speed_mode,
                    led_channel.channel,
                    0,
                    UPDATING_FADE_TIME
                );
                ledc_fade_start(
                    led_channel.speed_mode,
                    led_channel.channel,
                    LEDC_FADE_NO_WAIT
                );
            }
            else if (current_pattern_internal == CurrentPattern_Ringing_Ringing)
            {
                ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);
            }
            else if (current_pattern_internal == CurrentPattern_Ringing_Sending)
            {
                ESP_LOGI(TAG, "next step in the ringing sending pattern: %d", current_pattern_progress);

                if (current_pattern_progress == 0)
                {
                    ledc_set_fade_with_time(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LED_MAX_DUTY,
                        RINGING_FADE_TIME
                    );
                    ledc_fade_start(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LEDC_FADE_NO_WAIT
                    );
                }
                else
                {
                    ledc_set_fade_with_time(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LED_MEDIUM_DUTY,
                        RINGING_FADE_TIME
                    );
                    ledc_fade_start(
                        led_channel.speed_mode,
                        led_channel.channel,
                        LEDC_FADE_NO_WAIT
                    );
                }

                current_pattern_progress = (current_pattern_progress + 1) % 2;
            }
        }

        if (xEventGroupGetBits(current_pattern_events) & CURRENT_PATTERN_UPDATED)
        {
            xEventGroupClearBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

            ESP_LOGI(TAG, "pattern update event triggered");

            ESP_LOGI(TAG, "locking current_pattern_semaphore to get new pattern...");

            if (xSemaphoreTake(current_pattern_semaphore, 0))
            {
                if (current_pattern_internal == current_pattern)
                {
                    xSemaphoreGive(current_pattern_semaphore);
                }
                else
                {
                    current_pattern_internal = current_pattern;
                    current_pattern_data_internal = current_pattern_data;

                    current_pattern_progress = -1;

                    xSemaphoreGive(current_pattern_semaphore);

                    ledc_fade_stop(led_channel.speed_mode, led_channel.channel);

                    if (current_pattern_internal == CurrentPattern_Error)
                    {
                        if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                        {
                            current_pattern_internal = CurrentPattern_Off;
                            current_pattern = CurrentPattern_Off;

                            xSemaphoreGive(current_pattern_semaphore);
                        }

                        ESP_LOGI(TAG, "pattern set to CurrentPattern_Error, running blocking effect...");

                        // yes this should block, we need the error code to fully play out to be useful

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, 0);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, 0);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MEDIUM_DUTY);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, 0);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        for (int i = ERROR_MAX_BITS - 1; i >= 0; i--)
                        {
                            if ((current_pattern_data_internal >> i) & 1)
                            {
                                ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                                ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                                vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);
                            }
                            else
                            {
                                ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MEDIUM_DUTY);
                                ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                                vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);
                            }

                            ledc_set_duty(led_channel.speed_mode, led_channel.channel, 0);
                            ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                            vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);
                        }

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MEDIUM_DUTY);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);
                        vTaskDelay(ERROR_HOLD_TIME / portTICK_PERIOD_MS);

                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            0,
                            ERROR_HOLD_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );

                        xSemaphoreTake(fade_complete_semaphore, portMAX_DELAY);

                        xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_COMPLETE);

                        ESP_LOGI(TAG, "blocking effect done, sent pattern complete event");
                    }
                    else if (current_pattern_internal == CurrentPattern_WifiDisconnected)
                    {
                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                        vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LED_MEDIUM_DUTY,
                            WIFI_DISCONNECTED_SHORT_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );

                        current_pattern_progress = 1;
                    }
                    else if (current_pattern_internal == CurrentPattern_Updating)
                    {
                        current_pattern_progress = 0;

                        ledc_set_duty(led_channel.speed_mode, led_channel.channel, LED_MAX_DUTY);
                        ledc_update_duty(led_channel.speed_mode, led_channel.channel);

                        vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            0,
                            UPDATING_FADE_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );
                    }
                    else if (current_pattern_internal == CurrentPattern_Ringing_Ringing)
                    {
                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LED_MAX_DUTY,
                            RINGING_FADE_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );
                        current_pattern_progress = 0;
                    }
                    else if (current_pattern_internal == CurrentPattern_Ringing_Sending)
                    {
                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LED_MAX_DUTY,
                            RINGING_FADE_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );

                        current_pattern_progress = 1;
                    }
                    else
                    {
                        ledc_set_fade_with_time(
                            led_channel.speed_mode,
                            led_channel.channel,
                            0,
                            LED_TEST_FADE_TIME
                        );
                        ledc_fade_start(
                            led_channel.speed_mode,
                            led_channel.channel,
                            LEDC_FADE_NO_WAIT
                        );
                    }
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
