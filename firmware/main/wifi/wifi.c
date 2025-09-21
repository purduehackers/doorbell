
#include "wifi.h"

#include "main.h"
#include "socket.h"
#include "status/status.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eap_client.h"

// TODO: pull from .env
// #define WIFI_USE_WPA2_PSK
// #define WIFI_SSID   "ssid"
// #define WIFI_PASS   "pass"
#define WIFI_USE_WPA2_ENTERPRISE
#define WIFI_SSID           "test"
#define WIFI_EAP_USERNAME   "test"
#define WIFI_EAP_PASSWORD   "test"
#define WIFI_EAP_IDENTITY   "test"
#define WIFI_EAP_DOMAIN     "test"

static const char *TAG = "wifi";

// ok so turns out refusing to sleep without a wifi connection is a bad idea
// static bool took_sleep_inhibit;

static void wifi_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED)
        {
            ESP_LOGI(TAG, "connected to access point, waiting for ip...");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGI(TAG, "failed to connect to an access point");

            // if (!took_sleep_inhibit)
            // {
            //     take_sleep_inhibit();
            //     took_sleep_inhibit = true;
            // }

            stop_socket();

            update_wifi_status(WifiStatus_Connecting);

            vTaskDelay(1000 / portTICK_PERIOD_MS);

            esp_wifi_connect();

            ESP_LOGI(TAG, "retrying connection...");
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

            ESP_LOGI(TAG, "connected to access point, got ip: " IPSTR, IP2STR(&event->ip_info.ip));

            // if (took_sleep_inhibit)
            // {
            //     return_sleep_inhibit();
            //     took_sleep_inhibit = false;
            // }

            start_socket();

            update_wifi_status(WifiStatus_Connected);
        }
        else if (event_id == IP_EVENT_STA_LOST_IP)
        {
            ESP_LOGI(TAG, "connected to access point, lost ip");

            // if (!took_sleep_inhibit)
            // {
            //     take_sleep_inhibit();
            //     took_sleep_inhibit = true;
            // }

            stop_socket();

            update_wifi_status(WifiStatus_Connecting);
        }
    }
}

void start_wifi()
{
    init_socket_state();

    ESP_LOGI(TAG, "initializing netif...");

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_LOGI(TAG, "initializing wifi station...");

    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "initializing wifi...");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(TAG, "initializing event handlers...");

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &instance_any_id
        )
    );
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &instance_got_ip
        )
    );

    ESP_LOGI(TAG, "setting wifi config...");

    #ifdef WIFI_USE_WPA2_PSK
    ESP_LOGI(TAG, "using PSK...");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_USE_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    #endif

    #ifdef WIFI_USE_WPA2_ENTERPRISE
    ESP_LOGI(TAG, "using WPA2 Enterprise with PEAP...");

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_eap_client_set_identity((uint8_t *) WIFI_EAP_IDENTITY, strlen(WIFI_EAP_IDENTITY)));
    ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t *) WIFI_EAP_USERNAME, strlen(WIFI_EAP_USERNAME)));
    ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t *) WIFI_EAP_PASSWORD, strlen(WIFI_EAP_PASSWORD)));
    ESP_ERROR_CHECK(esp_eap_client_set_domain_name(WIFI_EAP_DOMAIN));

    ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
    #endif

    ESP_LOGI(TAG, "starting wifi...");

    // took_sleep_inhibit = true;

    // take_sleep_inhibit();

    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // i give up on this for now, it breaks wifi completely
    // esp_sleep_enable_wifi_beacon_wakeup();

    ESP_LOGI(TAG, "initialization finished");
}

void prepare_wifi_for_sleep()
{
    stop_socket();
    esp_wifi_stop();
}

void wake_wifi_from_sleep()
{
    esp_wifi_start();
}
