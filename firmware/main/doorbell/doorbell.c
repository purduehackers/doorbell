#include "doorbell.h"

#include "hal/gpio_types.h"
#include "status/status.h"
#include "wifi/wifi.h"
#include "wifi/socket.h"
#include "main.h"

#include <string.h>

#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "doorbell";

static TaskHandle_t doorbell_thread_handle;

EventGroupHandle_t doorbell_events;

static bool took_sleep_inhibit;

void IRAM_ATTR doorbell_rung_interrupt(void *args)
{
    xEventGroupSetBitsFromISR(doorbell_events, DOORBELL_PRESSED, NULL);
}

void doorbell_thread_entrypoint(void * arg)
{
    while (1)
    {
        if (xEventGroupWaitBits(doorbell_events, DOORBELL_PRESSED, pdFALSE, pdFALSE, portMAX_DELAY) & DOORBELL_PRESSED)
        {
            ESP_LOGI(TAG, "doorbell rung");

            if (!took_sleep_inhibit)
            {
                take_sleep_inhibit();
                took_sleep_inhibit = true;
            }

            xEventGroupClearBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            update_ringing_status(RingingStatus_Sending);
            ring_doorbell(false);

            xEventGroupWaitBits(doorbell_events, DOORBELL_FINISHED_RINGING, pdTRUE, pdFALSE, portMAX_DELAY);

            xEventGroupClearBits(doorbell_events, DOORBELL_PRESSED);
            xEventGroupClearBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            if (took_sleep_inhibit)
            {
                return_sleep_inhibit();
                took_sleep_inhibit = false;
            }
        }
    }
}

void start_doorbell()
{
    ESP_LOGI(TAG, "initializing state...");

    doorbell_events = xEventGroupCreate();

    took_sleep_inhibit = false;

    ESP_LOGI(TAG, "initializing io...");

    esp_rom_gpio_pad_select_gpio(DOORBELL_PIN);

    gpio_config_t config = {
        .pin_bit_mask = BIT64(DOORBELL_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_HIGH_LEVEL
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(DOORBELL_PIN, doorbell_rung_interrupt, NULL));
    ESP_ERROR_CHECK(gpio_wakeup_enable(DOORBELL_PIN, GPIO_INTR_HIGH_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    ESP_LOGI(TAG, "starting thread...");

    xTaskCreate(
        doorbell_thread_entrypoint,
        "doorbell",
        10000,
        NULL,
        tskIDLE_PRIORITY,
        &doorbell_thread_handle
    );

    ESP_LOGI(TAG, "initialization finished");
}

void prepare_doorbell_for_sleep()
{
    // we don't want to trigger a double-interrupt
    gpio_isr_handler_remove(DOORBELL_PIN);

    gpio_hold_en(DOORBELL_PIN);
}

void wake_doorbell_from_sleep(bool triggered)
{
    if (triggered)
    {
        if (!took_sleep_inhibit)
        {
            take_sleep_inhibit();
            took_sleep_inhibit = true;
        }

        xEventGroupClearBits(doorbell_events, DOORBELL_FINISHED_RINGING);

        update_ringing_status(RingingStatus_Sending);
        ring_doorbell(true);

        xEventGroupWaitBits(doorbell_events, DOORBELL_FINISHED_RINGING, pdTRUE, pdFALSE, portMAX_DELAY);

        xEventGroupClearBits(doorbell_events, DOORBELL_PRESSED);
        xEventGroupClearBits(doorbell_events, DOORBELL_FINISHED_RINGING);

        if (took_sleep_inhibit)
        {
            return_sleep_inhibit();
            took_sleep_inhibit = false;
        }
    }

    gpio_hold_dis(DOORBELL_PIN);

    gpio_isr_handler_add(DOORBELL_PIN, doorbell_rung_interrupt, NULL);
}
