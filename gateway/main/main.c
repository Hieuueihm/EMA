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
#include "esp_timer.h"
#include "lora.h"
#include "fake_node.h"
#include "timesync.h"
#include "lora_packet.h"
#include "esp_random.h"
#include "esp_mac.h"
#include <sys/time.h>
#include "cJSON.h"
#include "reverse_geocode.h"
#include "main.h"
#include "esp_task_wdt.h"

static int64_t s_ap_expire_ms = 0;
static const int64_t AP_LIFETIME_MS = 180000;
#define LORA_FREQUENCY 433E6
#define RESP_QUEUE_LEN 64

QueueHandle_t g_q_resp = NULL;
uint8_t g_gwid[6] = {0};

static SemaphoreHandle_t g_lora_mutex = NULL;
static const char *TAG = "APP";
static bool lora_inited = true;
#define LED_GPIO 25 //
#define LED_ON() gpio_set_level(LED_GPIO, 1)
#define LED_OFF() gpio_set_level(LED_GPIO, 0)
#define WIFI_RETRY_STA_MS 60000

volatile bool ap_mode = false;
extern wifi_state_t wifi_state;

typedef struct
{
    uint16_t device_id;
    sensor_payload_t s;
    const char *province;
} sensor_event_t;

#define MQTT_QUEUE_LEN 16
static QueueHandle_t g_q_mqtt = NULL;

static bool send_header_only(const lora_header_t *hdr, bool is_random_seq16)
{
    uint8_t tx[LORA_HEADER_LEN];
    uint16_t n = lora_write_header_only(tx, sizeof(tx), hdr, is_random_seq16);
    if (!n)
        return false;

    xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
    bool ok = lora_send_packet(tx, n);
    lora_receive();
    xSemaphoreGive(g_lora_mutex);
    return ok;
}
static void get_gateway_id(uint8_t out6[6])
{
    // Lấy MAC STA (6 byte)
    esp_read_mac(out6, ESP_MAC_WIFI_STA);
}

static cJSON *build_values_obj(const sensor_payload_t *s)
{
    cJSON *obj = cJSON_CreateObject();
    if (!obj)
        return NULL;

    cJSON_AddNumberToObject(obj, "temperature", s->temperature);
    cJSON_AddNumberToObject(obj, "humidity", s->humidity);
    cJSON_AddNumberToObject(obj, "co_ppm", s->co_ppm);
    cJSON_AddNumberToObject(obj, "uvi", s->uvi);
    cJSON_AddNumberToObject(obj, "pm25", s->pm25);
    cJSON_AddNumberToObject(obj, "pm10", s->pm10);
    return obj;
}
static inline void make_dev_name(char *out, size_t cap, uint16_t dev_id)
{
    snprintf(out, cap, "NODE_%04X", dev_id);
}
static cJSON *build_attrs_obj(uint16_t device_id,
                              float lat, float lon, uint8_t buzzer,
                              const char *province)
{
    cJSON *obj = cJSON_CreateObject();
    if (!obj)
        return NULL;
    char id_str[8];
    sprintf(id_str, "%04X", device_id);
    cJSON_AddStringToObject(obj, "device_id", id_str);
    cJSON_AddNumberToObject(obj, "lat", lat);
    cJSON_AddNumberToObject(obj, "lon", lon);
    cJSON_AddNumberToObject(obj, "buzzer", buzzer);
    cJSON_AddStringToObject(obj, "province", province ? province : "");

    return obj;
}
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
void rx_task(void *pvParameters)
{
    uint8_t buf[256];

    xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
    if (!lora_init())
    {
        lora_inited = false;
        xSemaphoreGive(g_lora_mutex);
        ESP_LOGE(TAG, "LoRa init failed");
        vTaskDelete(NULL);
        return;
    }
    lora_set_frequency(LORA_FREQUENCY);
    lora_set_spreading_factor(12);
    lora_set_bandwidth(125E3);
    lora_enable_crc();
    lora_receive(); // arm RX lần đầu
    xSemaphoreGive(g_lora_mutex);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5));

        xSemaphoreTake(g_lora_mutex, portMAX_DELAY);

        if (!lora_received())
        {
            xSemaphoreGive(g_lora_mutex);
            continue;
        }

        int len = lora_receive_packet(buf, sizeof(buf));
        if (len < LORA_HEADER_LEN)
        {
            lora_receive(); // re-arm nếu rác
            xSemaphoreGive(g_lora_mutex);
            continue;
        }

        lora_header_t rxh;
        bool ok_hdr = lora_parse_header(buf, (uint16_t)len, &rxh);
        if (!ok_hdr)
        {
            ESP_LOGW(TAG, "Parse header fail (<%d), drop", LORA_HEADER_LEN);
            lora_receive(); // re-arm
            xSemaphoreGive(g_lora_mutex);
            continue;
        }

        int payload_len = len - LORA_HEADER_LEN;
        ESP_LOGI(TAG, "pkt gwid = %02X:%02X:%02X:%02X:%02X:%02X",
                 rxh.gateway_id[0], rxh.gateway_id[1], rxh.gateway_id[2],
                 rxh.gateway_id[3], rxh.gateway_id[4], rxh.gateway_id[5]);

        switch (rxh.msg_type)
        {
        case MSG_HELLO:
        {
            lora_header_t resp = rxh;
            resp.msg_type = MSG_HELLO_RESP;
            memcpy(resp.gateway_id, g_gwid, 6);
            resp.ack = 1;
            xSemaphoreGive(g_lora_mutex);
            if (xQueueSend(g_q_resp, &resp, 0) != pdTRUE)
            {
                ESP_LOGW(TAG, "RESP queue full (HELLO_RESP) dev=0x%04X seq=0x%04X", rxh.device_id, rxh.seq16);
            }
            else
            {
                ESP_LOGI(TAG, "HELLO enq: dev=0x%04X seq=0x%04X", rxh.device_id, rxh.seq16);
            }
            xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
            lora_receive();
            xSemaphoreGive(g_lora_mutex);
            break;
        }

        case MSG_DATA_FNODE:
        {
            bool equal = memcmp(rxh.gateway_id, g_gwid, 6) == 0;
            if (equal)
            {
                const uint8_t *pl = &buf[LORA_HEADER_LEN];
                if (payload_len >= SENSOR_PAYLOAD_MIN_LEN)
                {
                    sensor_payload_t s;
                    int o = 0;
                    s.temperature = rd_f32_be(&pl[o]);
                    o += 4;
                    s.humidity = rd_f32_be(&pl[o]);
                    o += 4;
                    s.co_ppm = rd_f32_be(&pl[o]);
                    o += 4;
                    s.uvi = pl[o++];
                    s.pm25 = rd_u16_be(&pl[o]);
                    o += 2;
                    s.pm10 = rd_u16_be(&pl[o]);
                    o += 2;
                    s.buzzer = pl[o++];
                    s.latitude = rd_f32_be(&pl[o]);
                    o += 4;
                    s.longitude = rd_f32_be(&pl[o]);
                    o += 4;

                    ESP_LOGI(TAG, "DATA from dev=0x%04X seq=0x%04X | T=%.2f H=%.2f CO=%.2f UVI=%.2f PM2.5=%u PM10=%u Bz=%u Lat=%.6f Lon=%.6f",
                             rxh.device_id, rxh.seq16, s.temperature, s.humidity, s.co_ppm, s.uvi, s.pm25, s.pm10, s.buzzer, s.latitude, s.longitude);

                    xSemaphoreGive(g_lora_mutex);
                    sensor_event_t ev = {.device_id = rxh.device_id, .s = s};
                    if (xQueueSend(g_q_mqtt, &ev, 0) != pdTRUE)
                    {
                        ESP_LOGW(TAG, "MQTT queue full, drop dev=0x%04X", rxh.device_id);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "ENQ MQTT dev=0x%04X", rxh.device_id);
                    }

                    lora_header_t ack = rxh;
                    ack.msg_type = MSG_DATA_ACK;
                    memcpy(ack.gateway_id, g_gwid, 6);
                    ack.ack = 1;
                    if (xQueueSend(g_q_resp, &ack, 0) != pdTRUE)
                    {
                        ESP_LOGW(TAG, "RESP queue full (DATA_ACK) dev=0x%04X seq=0x%04X", rxh.device_id, rxh.seq16);
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "DATA payload too short (%d)", payload_len);
                    xSemaphoreGive(g_lora_mutex);
                }

                xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
                lora_receive();
                xSemaphoreGive(g_lora_mutex);
            }
            else
            {
                lora_receive();
                xSemaphoreGive(g_lora_mutex);
            }
            break;
        }

        case MSG_CTR_ACK:
            ESP_LOGI(TAG, "CTR_ACK received from dev=0x%04X seq=0x%04X", rxh.device_id, rxh.seq16);
            lora_receive();
            xSemaphoreGive(g_lora_mutex);
            break;

        default:
            lora_receive();
            xSemaphoreGive(g_lora_mutex);
            break;
        }
    }
}

static void resp_task(void *arg)
{
    lora_header_t hdr;
    while (1)
    {
        if (xQueueReceive(g_q_resp, &hdr, portMAX_DELAY) != pdTRUE)
            continue;

        bool ok = send_header_only(&hdr, false);
        // bool ok = 0;

        if (hdr.msg_type == MSG_HELLO_RESP)
        {
            ESP_LOGI(TAG, "HELLO_RESP -> dev=0x%04X seq=0x%04X %s",
                     hdr.device_id, hdr.seq16, ok ? "OK" : "FAIL");
        }
        else if (hdr.msg_type == MSG_DATA_ACK)
        {
            ESP_LOGI(TAG, "DATA_ACK    -> dev=0x%04X seq=0x%04X %s",
                     hdr.device_id, hdr.seq16, ok ? "OK" : "FAIL");
        }
        else if (hdr.msg_type == MSG_NODE_CTR)
        {
            ESP_LOGI(TAG, "NODE_CTR   -> dev=0x%04X seq=0x%04X  %s",
                     hdr.device_id, hdr.seq16, ok ? "OK" : "FAIL");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
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
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_LOGI(TAG, "Applied saved SSID from NVS");
    return (ssid[0] != '\0');
}

static void send_data_task(void *arg)
{
    mqtt_init();
    sensor_event_t ev;

    while (1)
    {
        if (!ap_mode && wifi_state == GOT_IP)
        {
            if (!mqtt_is_connected())
            {
                mqtt_start(1000);
            }
            else if (xQueueReceive(g_q_mqtt, &ev, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                char dev_name[32];
                make_dev_name(dev_name, sizeof(dev_name), ev.device_id);
                cJSON *values = build_values_obj(&ev.s);
                if (values)
                {
                    bool ok_t = gw_publish_telemetry(dev_name, values);
                    if (!ok_t)
                    {
                        ESP_LOGW(TAG, "gw_publish_telemetry FAIL dev=%s", dev_name);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Telemetry published dev=%s", dev_name);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "build_values_obj NULL");
                }
                const char *province = NULL;
                reverse_geocode_province(ev.s.latitude, ev.s.longitude, &province);
                cJSON *attrs = build_attrs_obj(ev.device_id,
                                               ev.s.latitude, ev.s.longitude,
                                               ev.s.buzzer, province);

                if (attrs)
                {
                    bool ok_a = gw_publish_attributes(dev_name, attrs);
                    if (!ok_a)
                    {
                        ESP_LOGW(TAG, "gw_publish_attributes FAIL dev=%s", dev_name);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Attributes published dev=%s", dev_name);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "build_attrs_obj NULL");
                }
            }
            else
            {
                ESP_LOGD(TAG, "AP mode or no IP - skipping publish cycle");
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
static void network_supervisor_task(void *arg)
{
    const uint32_t QUICK_RETRY_WAIT_MS = 10000;
    const int MAX_QUICK_RETRIES = 6;

    int quick_retries = 0;
    bool synced_time = false;

    while (1)
    {
        // ESP_LOGI(TAG, "wifi state: %d \r\n", wifi_state);
        if (!ap_mode)
        {
            if (wifi_state == GOT_IP)
            {

                vTaskDelay(pdMS_TO_TICKS(1000));
                if (synced_time == false)
                {
                    if (ntp_start())
                    {
                        synced_time = true;
                    }
                    else
                    {
                        synced_time = false;
                    }
                }
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
                    quick_retries = 0;
                    ESP_LOGW(TAG, "Too many STA retries → switch to AP portal");
                    wifi_sta_stop();
                    start_ap_and_server();
                    s_ap_expire_ms = (esp_timer_get_time() / 1000) + AP_LIFETIME_MS;
                    ap_mode = true;
                    synced_time = false;
                }
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else if (ap_mode)
        {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (s_ap_expire_ms > 0 && now_ms > s_ap_expire_ms)
            {
                ap_mode = false;
                wifi_sta_start_and_wait_ip(WIFI_RETRY_STA_MS);
                ESP_LOGW(TAG, "AP portal expired → try STA");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void app_main(void)
{
    esp_err_t err;
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
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

    if (!have_saved)
    {
        ESP_LOGW(TAG, "No saved Wi-Fi → start AP portal immediately");
        start_ap_and_server();
        s_ap_expire_ms = (esp_timer_get_time() / 1000) + AP_LIFETIME_MS;
        ap_mode = true;
    }
    get_gateway_id(g_gwid);
    ESP_LOGI(TAG, "GWID = %02X:%02X:%02X:%02X:%02X:%02X",
             g_gwid[0], g_gwid[1], g_gwid[2], g_gwid[3], g_gwid[4], g_gwid[5]);

    g_q_resp = xQueueCreate(RESP_QUEUE_LEN, sizeof(lora_header_t));
    g_q_mqtt = xQueueCreate(MQTT_QUEUE_LEN, sizeof(sensor_event_t));
    g_lora_mutex = xSemaphoreCreateMutex();
    configASSERT(g_q_resp && g_lora_mutex);

    xTaskCreate(rx_task, "rx_task", 7168, NULL, 5, NULL);
    xTaskCreate(resp_task, "resp_task", 6144, NULL, 4, NULL);

    xTaskCreate(send_data_task, "send_data_task", 4096, NULL, 1, NULL);
    xTaskCreate(network_supervisor_task, "network_supervisor_task", 4096, NULL, 2, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 3, NULL);
}
