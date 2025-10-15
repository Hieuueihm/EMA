#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "lora.h"

#define WIFI_SSID "Việt Anh"
#define WIFI_PASS "25051992"

#define MQTT_BROKER_URI "mqtt://demo.thingsboard.io:1883"
#define ACCESS_TOKEN_DEVICE_1 "GCw2ijLxooFWzmNtebI5"
#define ACCESS_TOKEN_DEVICE_2 "rytkkFgK9Lx0gM93HHpe"

#define LORA_FREQUENCY 433E6

static const char *TAG = "ThingsBoard";

typedef struct
{
    char mac_address[18];
    float temperature;
    float heart_rate;
    float spo2;
} sensor_data_t;

static const struct
{
    const char *mac_address;
    const char *access_token;
} device_mapping[] = {
    {"24:DC:C3:46:06:88", ACCESS_TOKEN_DEVICE_1},
    {"EC:64:C9:86:DC:EC", ACCESS_TOKEN_DEVICE_2}};
#define DEVICE_MAPPING_COUNT (sizeof(device_mapping) / sizeof(device_mapping[0]))

esp_mqtt_client_handle_t client;

// Initialize Wi-Fi
void wifi_init()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS}};

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (event_id == MQTT_EVENT_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to ThingsBoard");
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected from ThingsBoard");
    }
}

void mqtt_init(const char *access_token)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = access_token};

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
}

void send_data_to_thingsboard(sensor_data_t *data)
{
    const char *access_token = NULL;
    for (size_t i = 0; i < DEVICE_MAPPING_COUNT; i++)
    {
        if (strcmp(data->mac_address, device_mapping[i].mac_address) == 0)
        {
            access_token = device_mapping[i].access_token;
            break;
        }
    }

    mqtt_init(access_token);
    esp_mqtt_client_start(client);

    char payload[200];
    snprintf(payload, sizeof(payload), "{\"mac_address\":\"%s\",\"temperature\":%.2f,\"heart_rate\":%.2f,\"spo2\":%.2f}",
             data->mac_address, data->temperature, data->heart_rate, data->spo2);

    int msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", payload, 0, 1, 0);
    if (msg_id != -1)
    {
        ESP_LOGI(TAG, "Data sent successfully for %s, msg_id=%d, payload=%s", data->mac_address, msg_id, payload);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send data for %s", data->mac_address);
    }

    esp_mqtt_client_stop(client);
}

void sensor_task(void *pvParameters)
{
    uint8_t buf[256];
    int len;
    sensor_data_t data;

    if (!lora_init())
    {
        ESP_LOGE(TAG, "LoRa initialization failed.");
        vTaskDelete(NULL);
        return;
    }

    lora_set_frequency(LORA_FREQUENCY);
    lora_set_spreading_factor(12);
    lora_set_bandwidth(125E3);
    lora_enable_crc();

    while (1)
    {
        lora_receive();

        if (lora_received())
        {
            len = lora_receive_packet(buf, sizeof(buf));
            if (len > 0)
            {
                buf[len] = '\0';
                ESP_LOGI(TAG, "Received data: %s", (char *)buf);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    nvs_flash_init();
    wifi_init();
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}