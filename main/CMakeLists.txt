idf_component_register(SRCS "station_example_main.c" "wifi_connect.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip esp_timer
                    )