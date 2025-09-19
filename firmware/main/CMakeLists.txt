idf_component_register(
    SRCS "main.c" "doorbell/doorbell.c" "status/status.c" "status/pattern_driver_thread.c" "status/status_sync_thread.c" "wifi/wifi.c" "wifi/socket.c" "wifi/websocket_client/esp_websocket_client.c"
    PRIV_REQUIRES driver esp_wifi nvs_flash tcp_transport http_parser
    INCLUDE_DIRS "." "doorbell/" "status/" "wifi/" "wifi/websocket_client/"
)
