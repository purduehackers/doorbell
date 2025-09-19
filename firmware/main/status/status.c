#include "status.h"

#include "status_sync_thread.h"
#include "pattern_driver_thread.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "status";

static TaskHandle_t led_pattern_driver_thread_handle;
static TaskHandle_t led_status_sync_thread_handle;

ledc_timer_config_t led_timer;
ledc_channel_config_t led_channel;
ledc_cbs_t led_callbacks;

SemaphoreHandle_t fade_complete_semaphore;

bool IRAM_ATTR led_fade_end_interrupt(const ledc_cb_param_t *param, void *user_arg)
{
    BaseType_t taskAwoken = pdFALSE;

    if (param->event == LEDC_FADE_END_EVT)
    {
        SemaphoreHandle_t fade_complete_semaphore = (SemaphoreHandle_t) user_arg;

        xSemaphoreGiveFromISR(fade_complete_semaphore, &taskAwoken);
    }

    return (taskAwoken == pdTRUE);
}

void start_status()
{
    ESP_LOGI(TAG, "setting up timer...");

    led_timer = (ledc_timer_config_t) {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 2000,
        .speed_mode = LED_LS_MODE,
        .timer_num = LED_LS_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&led_timer));

    ESP_LOGI(TAG, "setting up led channel...");

    led_channel = (ledc_channel_config_t) {
        .channel    = LED_LS_CH2_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_LS_CH2_GPIO,
        .speed_mode = LED_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LED_LS_TIMER,
        .flags.output_invert = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&led_channel));

    ESP_LOGI(TAG, "starting led fade function...");

    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    ESP_LOGI(TAG, "initializing state...");

    fade_complete_semaphore = xSemaphoreCreateCounting(1, 0);

    current_pattern = CurrentPattern_StartingUp;
    current_pattern_data = 0;

    current_pattern_semaphore = xSemaphoreCreateMutex();
    current_pattern_events = xEventGroupCreate();

    system_ready = false;
    indicating_error = 0;
    indicating_wifi_status = WifiStatus_Connecting;
    indicating_updating = false;
    indicating_ringing = RingingStatus_Off;

    status_state_semaphore = xSemaphoreCreateMutex();
    status_state_events = xEventGroupCreate();

    ESP_LOGI(TAG, "registering callbacks...");

    led_callbacks = (ledc_cbs_t) {
        .fade_cb = led_fade_end_interrupt
    };
    ESP_ERROR_CHECK(ledc_cb_register(led_channel.speed_mode, led_channel.channel, &led_callbacks, (void *) fade_complete_semaphore));

    ESP_LOGI(TAG, "starting threads...");

    xTaskCreate(
        led_pattern_driver_thread_entrypoint,
        "lpdt",
        10000,
        NULL,
        tskIDLE_PRIORITY,
        &led_pattern_driver_thread_handle
    );
    xTaskCreate(
        led_status_sync_thread_entrypoint,
        "lsst",
        10000,
        NULL,
        tskIDLE_PRIORITY,
        &led_status_sync_thread_handle
    );

    ESP_LOGI(TAG, "initialization finished");
}

void ready_status()
{
    ESP_LOGI(TAG, "acquiring status_state_semaphore lock to set system ready...");

    if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
    {
        system_ready = true;

        xSemaphoreGive(status_state_semaphore);
        xEventGroupSetBits(status_state_events, STATUS_STATE_UPDATED);

        ESP_LOGI(TAG, "set system ready");
    }
}

void update_ringing_status(enum RingingStatus ringing)
{
    ESP_LOGI(TAG, "acquiring status_state_semaphore lock to set ringing state...");

    if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
    {
        indicating_ringing = ringing;

        xSemaphoreGive(status_state_semaphore);
        xEventGroupSetBits(status_state_events, STATUS_STATE_UPDATED);

        ESP_LOGI(TAG, "set ringing state");
    }
}

void update_updating_status(bool updating)
{
    ESP_LOGI(TAG, "acquiring status_state_semaphore lock to set updating state...");

    if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
    {
        indicating_updating = updating;

        xSemaphoreGive(status_state_semaphore);
        xEventGroupSetBits(status_state_events, STATUS_STATE_UPDATED);

        ESP_LOGI(TAG, "set updating state");
    }
}

void update_wifi_status(enum WifiStatus wifi_status)
{
    ESP_LOGI(TAG, "acquiring status_state_semaphore lock to set wifi state...");

    if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
    {
        indicating_wifi_status = wifi_status;

        xSemaphoreGive(status_state_semaphore);
        xEventGroupSetBits(status_state_events, STATUS_STATE_UPDATED);

        ESP_LOGI(TAG, "set wifi state");
    }
}

void display_error(int error)
{
    ESP_LOGI(TAG, "acquiring status_state_semaphore lock to set display error...");

    if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
    {
        indicating_error = error;

        xSemaphoreGive(status_state_semaphore);
        xEventGroupSetBits(status_state_events, STATUS_STATE_UPDATED);

        ESP_LOGI(TAG, "set display error");
    }
}

void prepare_status_for_sleep()
{
    ledc_set_duty(led_channel.speed_mode, led_channel.channel, 0);
    ledc_update_duty(led_channel.speed_mode, led_channel.channel);

    vTaskDelay(LED_SAFE_PWM_CYCLE_DELAY / portTICK_PERIOD_MS);

    gpio_hold_en(LED_LS_CH2_GPIO);
}

void wake_status_from_sleep()
{
    gpio_hold_dis(LED_LS_CH2_GPIO);
}
