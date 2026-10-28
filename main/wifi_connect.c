/*
Librería de conexión al WiFi para el ESP32

Portable a otros proyectos de ESP32

// TODO Probar que esto es cierto
Solo hay que incluir el archivo wifi_connect.h y llamar a la función wifi_connect_start().

Hecha por: Daniel Calbet Campillo en octubre de 2026
*/
#include "esp_timer.h"
#include "wifi_connect.h"
#include "main.h"

// WIFI DECLARATIONS

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "wifi station";

static int s_retry_num = 0;


// TIMER DECLARATIONS
static esp_timer_handle_t s_retry_timer;

static void retry_timer_cb(void *arg)
{
    esp_wifi_connect();
}

// TIMER FUNCTIONS
static void schedule_reconnect(void)
{
    /* 1, 2, 4, 8, 16, 30, 30... segundos */
    uint32_t delay_s = 1u << (s_retry_num < 5 ? s_retry_num : 5);
    if (delay_s > 30) delay_s = 30;
    s_retry_num++;

    ESP_LOGW(TAG, "Reintentando en %lu s (intento %d)",
             (unsigned long)delay_s, s_retry_num);
    esp_timer_stop(s_retry_timer);               /* por si ya estaba en marcha */
    esp_timer_start_once(s_retry_timer, (uint64_t)delay_s * 1000000ULL);
}


// WIFI EVENT HANDLER
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT){
        // Repaso por todos los eventos de WiFi
        switch(event_id) {
            //Comienzo de la secuencia
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                esp_wifi_connect();
                break;
            // Si no consigue conectarse, se reintenta hasta EXAMPLE_ESP_MAXIMUM_RETRY veces. Si tampoco lo consigue así, se llama a schedule_reconnect() para reintentar más tarde (en 30 segundos).    
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "retry to connect to the AP (intento %d)", s_retry_num);
                } else {
                    wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;
                    ESP_LOGW(TAG, "Desconectado (motivo %d)", d->reason);
                    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                    ESP_LOGI(TAG,"connect to the AP fail."); 
                    schedule_reconnect();
                }
                break;
            default:
                break;
            }
    // Si se ha conseguido conectar y obtener IP, se resetea el contador de reintentos y se pone el bit WIFI_CONNECTED_BIT en el event group.        
    }else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

}


/*Init NVS*/
static void initNVS()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }
}

/* Init wifi station mode*/
void wifi_init_sta(void)
{
    initNVS();

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if auth mode threshold equals WIFI_AUTH_OPEN
             * and password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_OPEN, // Si se pone contraseña se puede comprobar si se ha puesto contraseña o no.
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
#ifdef CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT
            .disable_wpa3_compatible_mode = 0,
#endif
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Create timer for retrying connection
    const esp_timer_create_args_t targs = {
        .callback = retry_timer_cb,
        .name = "wifi_retry",
    };
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_retry_timer)); 
    ESP_LOGI(TAG, "Timer created for retrying connection.");
}


/* Wifi connect start 
    Waits until the connection is established (WIFI_CONNECTED_BIT) and retries if it fails.
     * The bits are set by event_handler() (see above) 
*/
void wifi_connect_start()
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}