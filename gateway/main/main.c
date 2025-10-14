#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_sta.h"
#include "wifi_ap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "mqtt.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "APP";
#define LED_GPIO 25 //
#define LED_ON() gpio_set_level(LED_GPIO, 1)
#define LED_OFF() gpio_set_level(LED_GPIO, 0)

volatile bool ap_mode = false;
extern wifi_state_t wifi_state;

static void led_task(void *arg)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1)
    {
        if (ap_mode)
        {
            LED_ON();
            vTaskDelay(pdMS_TO_TICKS(2000));
            LED_OFF();
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else if (wifi_state == GOT_IP)
        {

            LED_ON();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else if (wifi_state == DISCONNECTED)
        {

            LED_ON();
            vTaskDelay(pdMS_TO_TICKS(500));
            LED_OFF();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else if (wifi_state == CONNECTED)
        {
            LED_ON();
            vTaskDelay(pdMS_TO_TICKS(200));
            LED_OFF();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

static bool apply_saved_sta_config(void)
{
    char ssid[64] = {0}, pass[96] = {0};
    if (!wifi_nvs_load(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        return false;
    }
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // đủ dùng

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_LOGI(TAG, "Applied saved SSID from NVS");
    return (ssid[0] != '\0');
}

static void data_task(void *arg)
{

    mqtt_init();
    const TickType_t PUB_PERIOD_MS = pdMS_TO_TICKS(2000);
    TickType_t last = xTaskGetTickCount();

    while (1)
    {
        if (!ap_mode && wifi_state == GOT_IP)
        {
            if (mqtt_is_connected())
            {
                cJSON *vals = cJSON_CreateObject();
                cJSON_AddNumberToObject(vals, "temperature", 25.3);
                gw_connect_device("NODE_B");
                if (gw_publish_telemetry("NODE_B", vals) != false)
                {
                    ESP_LOGI(TAG, "Published telemetry");
                }
            }
            else
            {
                mqtt_start(1000);
            }
        }

        vTaskDelayUntil(&last, PUB_PERIOD_MS);
    }
}

static void network_supervisor_task(void *arg)
{
    const uint32_t QUICK_RETRY_WAIT_MS = 10000;
    const int MAX_QUICK_RETRIES = 6;

    int quick_retries = 0;

    while (1)
    {
        if (!ap_mode)
        {
            if (wifi_state == GOT_IP)
            {

                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if (wifi_state == CONNECTED || wifi_state == DISCONNECTED)
            {
                bool ok = wifi_sta_start_and_wait_ip(QUICK_RETRY_WAIT_MS);
                if (ok)
                {
                    ESP_LOGI(TAG, "Reconnected and got IP");
                    quick_retries = 0;
                    continue;
                }
                quick_retries++;
                ESP_LOGW(TAG, "STA retry %d/%d failed", quick_retries, MAX_QUICK_RETRIES);

                if (quick_retries >= MAX_QUICK_RETRIES)
                {
                    ESP_LOGW(TAG, "Too many STA retries → switch to AP portal");
                    start_ap_and_server();
                    ap_mode = true;
                }
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void app_main(void)
{

    esp_log_level_set("MQTT", ESP_LOG_DEBUG);

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(err);
    // wifi_nvs_reset_wifi();

    if (!esp_netif_create_default_wifi_sta())
    {
        ESP_LOGE(TAG, "Create STA failed");
        abort();
    }
    if (!esp_netif_create_default_wifi_ap())
    {
        ESP_LOGE(TAG, "Create AP failed");
        abort();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    bool have_saved = apply_saved_sta_config();

    if (have_saved)
    {
        ESP_LOGI(TAG, "Try STA first (60s)...");
        bool ok = wifi_sta_start_and_wait_ip(60000);
        if (ok)
        {
            ESP_LOGI(TAG, "Wi-Fi OK → run normally");
            ap_mode = false;
        }
        else
        {
            ESP_LOGW(TAG, "No IP within 60s → start AP portal");
            wifi_sta_stop();
            start_ap_and_server();
            ap_mode = true;
        }
    }
    else
    {
        ESP_LOGW(TAG, "No saved Wi-Fi → start AP portal immediately");
        start_ap_and_server();
        ap_mode = true;
    }

    // Tasks
    xTaskCreate(data_task, "data_task", 4096, NULL, 3, NULL);
    xTaskCreate(network_supervisor_task, "network_supervisor_task", 4096, NULL, 2, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
}
