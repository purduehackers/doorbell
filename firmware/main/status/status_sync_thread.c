#include "status_sync_thread.h"

#include "pattern_driver_thread.h"
#include "status.h"

#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "esp_log.h"

static const char *TAG = "status (led status sync thread)";

bool system_ready;

int indicating_error;
enum WifiStatus indicating_wifi_status;
bool indicating_updating;
enum RingingStatus indicating_ringing;

SemaphoreHandle_t status_state_semaphore;
EventGroupHandle_t status_state_events;

void led_status_sync_thread_entrypoint(void * arg)
{
    // priority:
    // display error
    // wifi connection
    // update
    // ringing status

    ESP_LOGI(TAG, "waiting for system ready...");

    while (1)
    {
        if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
        {
            if (!system_ready)
            {
                xSemaphoreGive(status_state_semaphore);
                xEventGroupWaitBits(status_state_events, STATUS_STATE_UPDATED, pdTRUE, pdFALSE, portMAX_DELAY);
            }
            else
            {
                xSemaphoreGive(status_state_semaphore);
                break;
            }
        }
        else
        {
            vTaskDelay(2 / portTICK_PERIOD_MS);
        }
    }

    ESP_LOGI(TAG, "acquiring current_pattern_semaphore lock to display pattern 0...");

    if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
    {
        current_pattern = 0;

        xSemaphoreGive(current_pattern_semaphore);
        xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

        ESP_LOGI(TAG, "displayed pattern 0");
    }

    while (1)
    {
        ESP_LOGI(TAG, "acquiring status_state_semaphore lock to read indicating statuses...");

        if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
        {
            int new_indicating_error = indicating_error;
            enum WifiStatus new_indicating_wifi_status = indicating_wifi_status;
            bool new_indicating_updating = indicating_updating;
            enum RingingStatus new_indicating_ringing = indicating_ringing;

            xSemaphoreGive(status_state_semaphore);

            ESP_LOGI(TAG, "finished reading indicating statuses");

            if (new_indicating_error != 0)
            {
                ESP_LOGI(TAG, "indicating error present, locking current_pattern_semaphore to set pattern to CurrentPattern_Error...");

                xEventGroupClearBits(current_pattern_events, CURRENT_PATTERN_COMPLETE);

                if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                {
                    current_pattern = CurrentPattern_Error;
                    current_pattern_data = new_indicating_error;

                    xSemaphoreGive(current_pattern_semaphore);
                    xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                    ESP_LOGI(TAG, "set pattern to CurrentPattern_Error and sent update");
                }

                ESP_LOGI(TAG, "acquiring status_state_semaphore lock to update indicating error...");

                if (xSemaphoreTake(status_state_semaphore, portMAX_DELAY))
                {
                    indicating_error = 0;

                    xSemaphoreGive(status_state_semaphore);

                    ESP_LOGI(TAG, "indicating error updated");
                }

                ESP_LOGI(TAG, "waiting for pattern complete event...");

                xEventGroupWaitBits(current_pattern_events, CURRENT_PATTERN_COMPLETE, pdTRUE, pdFALSE, portMAX_DELAY);
            }
            else if (new_indicating_wifi_status != WifiStatus_Connected)
            {
                ESP_LOGI(TAG, "indicating wifi status set, locking current_pattern_semaphore to set pattern to CurrentPattern_WifiDisconnected...");

                if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                {
                    current_pattern = CurrentPattern_WifiDisconnected;
                    current_pattern_data = 0;

                    xSemaphoreGive(current_pattern_semaphore);
                    xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                    ESP_LOGI(TAG, "set pattern to CurrentPattern_WifiDisconnected and sent update");
                }
            }
            else if (new_indicating_updating)
            {
                ESP_LOGI(TAG, "indicating updating set, locking current_pattern_semaphore to set pattern to CurrentPattern_Updating...");

                if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                {
                    current_pattern = CurrentPattern_Updating;
                    current_pattern_data = 0;

                    xSemaphoreGive(current_pattern_semaphore);
                    xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                    ESP_LOGI(TAG, "set pattern to CurrentPattern_Updating and sent update");
                }
            }
            else if (new_indicating_ringing != RingingStatus_Off)
            {
                if (new_indicating_ringing == RingingStatus_Ringing)
                {
                    ESP_LOGI(TAG, "indicating ringing status set, locking current_pattern_semaphore to set pattern to CurrentPattern_Ringing_Ringing...");

                    if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                    {
                        current_pattern = CurrentPattern_Ringing_Ringing;
                        current_pattern_data = 1;

                        xSemaphoreGive(current_pattern_semaphore);
                        xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                        ESP_LOGI(TAG, "set pattern to CurrentPattern_Ringing_Ringing and sent update");
                    }
                }
                else if (new_indicating_ringing == RingingStatus_Sending)
                {
                    ESP_LOGI(TAG, "indicating ringing status set, locking current_pattern_semaphore to set pattern to CurrentPattern_Ringing_Sending...");

                    if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                    {
                        current_pattern = CurrentPattern_Ringing_Sending;
                        current_pattern_data = 0;

                        xSemaphoreGive(current_pattern_semaphore);
                        xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                        ESP_LOGI(TAG, "set pattern to CurrentPattern_Ringing_Sending and sent update");
                    }
                }
            }
            else
            {
                ESP_LOGI(TAG, "no indicating status set, locking current_pattern_semaphore to set pattern to CurrentPattern_Off...");

                if (xSemaphoreTake(current_pattern_semaphore, portMAX_DELAY))
                {
                    current_pattern = CurrentPattern_Off;
                    current_pattern_data = 0;

                    xSemaphoreGive(current_pattern_semaphore);
                    xEventGroupSetBits(current_pattern_events, CURRENT_PATTERN_UPDATED);

                    ESP_LOGI(TAG, "set pattern to CurrentPattern_Off and sent update");
                }
            }
        }

        ESP_LOGI(TAG, "waiting for STATUS_STATE_UPDATED...");

        xEventGroupWaitBits(status_state_events, STATUS_STATE_UPDATED, pdTRUE, pdFALSE, portMAX_DELAY);
    }
}
