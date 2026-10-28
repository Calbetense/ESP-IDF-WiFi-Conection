#pragma once

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"

/* The event group allows multiple bits for each event, but we only care about one event:
 * - we are connected to the AP with an IP */
#define WIFI_CONNECTED_BIT BIT0

void wifi_init_sta(void);
void wifi_connect_start(void);