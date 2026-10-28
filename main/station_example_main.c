/*
Archivo principal del proyecto, donde se inicializa la conexión WiFi y se inicia la conexión a la red.
Hecho por: Daniel Calbet Campillo en octubre de 2026.

Este achivo está hecho a partir del ejemplo de ESP-IDF "WiFi station".

Base para construir proyectos con base Wifi.
*/

#include "main.h"

// Etiqueta de log para archivo MAIN
static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    /* Init wifi station*/
    wifi_init_sta();
    /* Start wifi connection */
    wifi_connect_start();
}
