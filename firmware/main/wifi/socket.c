#include "socket.h"

#include "doorbell.h"
#include "status/status.h"
#include "websocket_client/esp_websocket_client.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_tls.h"
#include "esp_log.h"

static const char *TAG = "socket";

EventGroupHandle_t websocket_events;

static esp_tls_cfg_t tls_config;

static esp_websocket_client_config_t websocket_config;
static esp_websocket_client_handle_t websocket_client;

static TimerHandle_t websocket_retry_timer;

void socket_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    if (event_base == WEBSOCKET_EVENTS)
    {
        if (event_id == WEBSOCKET_EVENT_ERROR)
        {
            ESP_LOGI(TAG, "socket error, queued restart...");

            stop_socket();

            xTimerReset(websocket_retry_timer, portMAX_DELAY);
        }
        else if (event_id == WEBSOCKET_EVENT_CONNECTED)
        {
            ESP_LOGI(TAG, "socket connected");
            xEventGroupSetBits(websocket_events, SOCKET_CONNECTED);
        }
        else if (event_id == WEBSOCKET_EVENT_DISCONNECTED || event_id == WEBSOCKET_EVENT_CLOSED)
        {
            ESP_LOGI(TAG, "socket disconnected");
            xEventGroupClearBits(websocket_events, SOCKET_CONNECTED);
        }
        else if (event_id == WEBSOCKET_EVENT_DATA)
        {
            ESP_LOGI(TAG, "socket message");

            esp_websocket_event_data_t message_event_data = *(esp_websocket_event_data_t*) event_data;

            if (message_event_data.op_code == 1 && message_event_data.payload_len > 0)
            {
                ESP_LOGI(TAG, "socket user message");

                char first_character = *(message_event_data.data_ptr + message_event_data.payload_offset);

                if (first_character == 't')
                {
                    ESP_LOGI(TAG, "socket message: ring true");

                    xEventGroupClearBits(doorbell_events, DOORBELL_FINISHED_RINGING);
                    update_ringing_status(RingingStatus_Ringing);
                }
                else if (first_character == 'f')
                {
                    ESP_LOGI(TAG, "socket message: ring false");

                    update_ringing_status(RingingStatus_Off);
                    xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);
                }
            }
        }
    }
    //
}

void websocket_retry_timer_expired_callback(TimerHandle_t expired_sleep_timer)
{
    ESP_LOGI(TAG, "socket restart triggered...");

    start_socket();
}

void init_socket_state()
{
    ESP_LOGI(TAG, "initializing socket state...");

    websocket_events = xEventGroupCreate();

    websocket_retry_timer = xTimerCreate(
        "websocket retry timer",
        5000 / portTICK_PERIOD_MS,
        pdFALSE,
        (void *) 0,
        websocket_retry_timer_expired_callback
    );

    xTimerStop(websocket_retry_timer, 0);
}

void start_socket()
{
    xTimerStop(websocket_retry_timer, 0);

    ESP_LOGI(TAG, "initializing socket config...");

    websocket_config = (esp_websocket_client_config_t) {
        .uri = "wss://api.purduehackers.com/doorbell",

        .user_agent = "PurdueHackers/Doorbell",

        .network_timeout_ms = 10000,
        .reconnect_timeout_ms = 1000,
        .disable_pingpong_discon = true,
        .disable_auto_reconnect = false,
        .enable_close_reconnect = true
    };

    websocket_client = esp_websocket_client_init(&websocket_config);

    if (websocket_client == NULL)
    {
        ESP_LOGI(TAG, "socket config failed! restart queued...");

        xEventGroupClearBits(websocket_events, SOCKET_CONNECTED);
        xEventGroupClearBits(websocket_events, SOCKET_READY);

        xTimerReset(websocket_retry_timer, portMAX_DELAY);

        return;
    }

    ESP_LOGI(TAG, "registering socket events...");

    if (esp_websocket_register_events(websocket_client, WEBSOCKET_EVENT_ANY, socket_event_handler, NULL) != 0)
    {
        ESP_LOGI(TAG, "socket event registration failed! restart queued...");

        xEventGroupClearBits(websocket_events, SOCKET_CONNECTED);
        xEventGroupClearBits(websocket_events, SOCKET_READY);

        esp_websocket_client_destroy(websocket_client);

        websocket_client = NULL;

        xTimerReset(websocket_retry_timer, portMAX_DELAY);

        return;
    }

    ESP_LOGI(TAG, "starting socket client...");

    if (esp_websocket_client_start(websocket_client) != 0)
    {
        ESP_LOGI(TAG, "socket client start failed! restart queued...");

        xEventGroupClearBits(websocket_events, SOCKET_CONNECTED);
        xEventGroupClearBits(websocket_events, SOCKET_READY);

        esp_websocket_unregister_events(websocket_client, WEBSOCKET_EVENT_ANY, socket_event_handler);

        esp_websocket_client_destroy(websocket_client);

        websocket_client = NULL;

        xTimerReset(websocket_retry_timer, portMAX_DELAY);

        return;
    }

    ESP_LOGI(TAG, "socket ready");

    xEventGroupSetBits(websocket_events, SOCKET_READY);
}

void stop_socket()
{
    xTimerStop(websocket_retry_timer, 0);

    if (websocket_client == NULL)
    {
        ESP_LOGI(TAG, "socket already stopped");

        return;
    }

    ESP_LOGI(TAG, "stopping socket...");

    xEventGroupClearBits(websocket_events, SOCKET_CONNECTED);
    xEventGroupClearBits(websocket_events, SOCKET_READY);

    esp_websocket_unregister_events(websocket_client, WEBSOCKET_EVENT_ANY, socket_event_handler);

    esp_websocket_client_stop(websocket_client);

    esp_websocket_client_destroy(websocket_client);

    ESP_LOGI(TAG, "socket stopped...");

    websocket_client = NULL;
}

enum RingError {
    RingError_NoSocket = 1,
    RingError_SocketNotReady = 2,
    RingError_SocketNotConnected = 3,
    RingError_SendFailed = 4,
};

void ring_doorbell(bool wait_for_connection)
{
    if (wait_for_connection)
    {
        ESP_LOGI(TAG, "waiting for connection...");

        if (!(xEventGroupWaitBits(websocket_events, SOCKET_CONNECTED, pdFALSE, pdFALSE, portMAX_DELAY) & SOCKET_CONNECTED))
        {
            ESP_LOGI(TAG, "connection wait failed!");

            display_error(RingError_SocketNotReady);
            update_ringing_status(RingingStatus_Off);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            return;
        }
    }
    else
    {
        ESP_LOGI(TAG, "checking connection...");

        if (websocket_client == NULL)
        {
            ESP_LOGI(TAG, "socket not initialized!");

            display_error(RingError_NoSocket);
            update_ringing_status(RingingStatus_Off);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            return;
        }

        EventBits_t event_group_bits = xEventGroupGetBits(websocket_events);

        if (!(event_group_bits & SOCKET_READY))
        {
            ESP_LOGI(TAG, "socket not ready!");

            display_error(RingError_SocketNotReady);
            update_ringing_status(RingingStatus_Off);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            return;
        }
        if (!(event_group_bits & SOCKET_CONNECTED))
        {
            ESP_LOGI(TAG, "socket not connected!");

            display_error(RingError_SocketNotConnected);
            update_ringing_status(RingingStatus_Off);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);

            return;
        }
    }

    ESP_LOGI(TAG, "sending message...");

    if (esp_websocket_client_send_text(websocket_client, "true", 4, 10000 / portTICK_PERIOD_MS) == -1)
    {
        ESP_LOGI(TAG, "failed to send message!");

        display_error(RingError_SendFailed);
        update_ringing_status(RingingStatus_Off);

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        xEventGroupSetBits(doorbell_events, DOORBELL_FINISHED_RINGING);

        return;
    }

    ESP_LOGI(TAG, "ring send success!");

    // we don't need this i think (events will get it)
    // update_ringing_status(RingingStatus_Ringing);
}
