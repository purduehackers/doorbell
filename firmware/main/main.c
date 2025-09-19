#include "main.h"

#include "doorbell/doorbell.h"
#include "status/status.h"
#include "wifi/wifi.h"

#include <string.h>

#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

static const char *TAG = "main";

static SemaphoreHandle_t sleep_inhibit_count;
static TimerHandle_t sleep_timer;

void light_sleep()
{
    prepare_doorbell_for_sleep();
    prepare_wifi_for_sleep();
    prepare_status_for_sleep();

    esp_light_sleep_start();

    uint32_t wakeup_causes = esp_sleep_get_wakeup_cause();

    wake_status_from_sleep();
    wake_wifi_from_sleep();
    wake_doorbell_from_sleep(wakeup_causes & BIT(ESP_SLEEP_WAKEUP_GPIO));
}

void sleep_timer_expired_callback(TimerHandle_t expired_sleep_timer)
{
    ESP_LOGI(TAG, "sleep triggered, good night!");

    xTimerStop(sleep_timer, 0);

    light_sleep();

    ESP_LOGI(TAG, "sleep finished, good morning!");

    xTimerReset(sleep_timer, 0);
}

void take_sleep_inhibit()
{
    ESP_LOGI(TAG, "sleep inhibit taken");

    xSemaphoreTake(sleep_inhibit_count, portMAX_DELAY);

    xTimerStop(sleep_timer, portMAX_DELAY);
}

void return_sleep_inhibit()
{
    ESP_LOGI(TAG, "sleep inhibit returned");

    xSemaphoreGive(sleep_inhibit_count);

    if (uxSemaphoreGetCount(sleep_inhibit_count) == MAX_SLEEP_HANDLES)
    {
        xTimerReset(sleep_timer, portMAX_DELAY);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL)
    {
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "start sleep service");

    sleep_inhibit_count = xSemaphoreCreateCounting(MAX_SLEEP_HANDLES, MAX_SLEEP_HANDLES);

    // sleep after 10 minutes: 600000
    // sleep after 20 seconds: 20000
    sleep_timer = xTimerCreate(
        "sleep timer",
        600000 / portTICK_PERIOD_MS,
        pdFALSE,
        (void *) 0,
        sleep_timer_expired_callback
    );
    xTimerStart(sleep_timer, portMAX_DELAY);

    take_sleep_inhibit();

    ESP_LOGI(TAG, "start status");

    start_status();

    ESP_LOGI(TAG, "start wifi");

    start_wifi();

    ESP_LOGI(TAG, "start doorbell");

    start_doorbell();

    ESP_LOGI(TAG, "system ready");

    ready_status();

    return_sleep_inhibit();
}
