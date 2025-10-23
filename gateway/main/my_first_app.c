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
#include "lora_packet.h"
#include "esp_random.h"
#include "esp_mac.h"

#define LORA_FREQUENCY 433E6
#define RESP_QUEUE_LEN 64
static const char *TAG = "ThingsBoard";

static QueueHandle_t g_q_resp = NULL;
static SemaphoreHandle_t g_lora_mutex = NULL;
static uint8_t g_gwid[6] = {0};
bool is_ctr_done = false;

static bool send_header_only(const lora_header_t *hdr, bool is_random_seq16)
{
    uint8_t tx[LORA_HEADER_LEN];
    uint16_t n = lora_write_header_only(tx, sizeof(tx), hdr, is_random_seq16);
    if (!n)
        return false;

    xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
    bool ok = lora_send_packet(tx, n);
    // nếu driver có wait_tx_done(timeout) hãy dùng thay delay
    vTaskDelay(pdMS_TO_TICKS(50));
    lora_receive(); // quay lại RX ngay
    xSemaphoreGive(g_lora_mutex);
    return ok;
}
static void get_gateway_id(uint8_t out6[6])
{
    // Lấy MAC STA (6 byte)
    esp_read_mac(out6, ESP_MAC_WIFI_STA);
}

void rx_task(void *pvParameters)
{
    uint8_t buf[256];
    int len;
    xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
    if (!lora_init())
    {
        xSemaphoreGive(g_lora_mutex);
        ESP_LOGE(TAG, "LoRa init failed");
        vTaskDelete(NULL);
        return;
    }

    lora_set_frequency(LORA_FREQUENCY);
    lora_set_spreading_factor(12);
    lora_set_bandwidth(125E3);
    lora_enable_crc();
    lora_receive();
    xSemaphoreGive(g_lora_mutex);

    while (1)
    {
        bool has_pkt = false;

        xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
        has_pkt = lora_received();
        xSemaphoreGive(g_lora_mutex);

        if (!has_pkt)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int len = 0;
        xSemaphoreTake(g_lora_mutex, portMAX_DELAY);
        len = lora_receive_packet(buf, sizeof(buf));
        xSemaphoreGive(g_lora_mutex);
        if (len < LORA_HEADER_LEN)
            continue;

        lora_header_t rxh;
        if (!lora_parse_header(buf, (uint16_t)len, &rxh))
        {
            ESP_LOGW(TAG, "Parse header fail (<%d), drop", LORA_HEADER_LEN);
            continue;
        }

        int payload_len = len - LORA_HEADER_LEN;
        ESP_LOGI(TAG, "pkt gwid = %02X:%02X:%02X:%02X:%02X:%02X",
                 rxh.gateway_id[0], rxh.gateway_id[1], rxh.gateway_id[2],
                 rxh.gateway_id[3], rxh.gateway_id[4], rxh.gateway_id[5]);
        switch (rxh.msg_type)
        {
        case MSG_HELLO:

            lora_header_t resp = rxh;
            resp.msg_type = MSG_HELLO_RESP;
            memcpy(resp.gateway_id, g_gwid, 6);
            resp.ack = 1; // HELLO_OK

            if (xQueueSend(g_q_resp, &resp, 0) != pdTRUE)
            {
                ESP_LOGW(TAG, "RESP queue full (HELLO_RESP) dev=0x%04X seq=0x%04X",
                         rxh.device_id, rxh.seq16);
            }
            else
            {
                ESP_LOGI(TAG, "HELLO enq: dev=0x%04X seq=0x%04X", rxh.device_id, rxh.seq16);
            }

            break;

        case MSG_DATA_FNODE:

            bool equal = true;
            for (int i = 0; i < 6; i++)
            {
                if (rxh.gateway_id[i] != g_gwid[i])
                {
                    equal = false;
                    break;
                }
            }
            if (equal)
            {
                ESP_LOGI(TAG, "DATA_FNODE: dev=0x%04X seq=0x%04X pl=%dB",
                         rxh.device_id, rxh.seq16, payload_len);
                const uint8_t *pl = &buf[LORA_HEADER_LEN];
                sensor_payload_t s;
                if (payload_len < SENSOR_PAYLOAD_MIN_LEN)
                {
                    ESP_LOGW(TAG, "DATA_FNODE: payload too short (%dB < %dB)", payload_len, SENSOR_PAYLOAD_MIN_LEN);
                }
                else
                {
                    int o = 0;
                    s.temperature = rd_f32_be(&pl[o]);
                    o += 4;
                    s.humidity = rd_f32_be(&pl[o]);
                    o += 4;
                    s.co_ppm = rd_f32_be(&pl[o]);
                    o += 4;
                    s.uvi = pl[o];
                    o += 1;
                    s.pm25 = rd_u16_be(&pl[o]);
                    o += 2;
                    s.pm10 = rd_u16_be(&pl[o]);
                    o += 2;
                    s.buzzer = pl[o];
                    o += 1;

                    ESP_LOGI(TAG,
                             "DATA from dev=0x%04X seq=0x%04X | T=%.2fC H=%.2f%% CO=%.2fppm UVI=%.2f PM2.5=%u PM10=%u Buzzer=%u",
                             rxh.device_id, rxh.seq16,
                             s.temperature, s.humidity, s.co_ppm, s.uvi, s.pm25, s.pm10, s.buzzer);

                    // (tuỳ chọn) Publish MQTT / ghi file / đẩy DB ở đây
                    // publish_sensor_mqtt(rxh.device_id, &s);  // ví dụ
                }

                // Chuẩn bị ACK
                lora_header_t ack = rxh;
                ack.msg_type = MSG_DATA_ACK;
                memcpy(ack.gateway_id, g_gwid, 6);
                ack.ack = 1; // ACK OK

                if (xQueueSend(g_q_resp, &ack, 0) != pdTRUE)
                {
                    ESP_LOGW(TAG, "RESP queue full (DATA_ACK) dev=0x%04X seq=0x%04X",
                             rxh.device_id, rxh.seq16);
                }
                if (s.buzzer == 1)
                {
                    lora_header_t ctr = rxh;
                    ctr.msg_type = MSG_NODE_CTR;
                    ctr.seq16 = esp_random() & 0xFFFF;
                    memcpy(ctr.gateway_id, g_gwid, 6);
                    ctr.ack = 1;

                    if (xQueueSend(g_q_resp, &ctr, 0) != pdTRUE)
                    {
                        ESP_LOGW(TAG, "RESP queue full (NODE_CTR) dev=0x%04X seq=0x%04X",
                                 rxh.device_id, rxh.seq16);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "NODE_CTR enq (mute) dev=0x%04X seq=0x%04X",
                                 rxh.device_id, rxh.seq16);
                    }
                }
            }
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
    }
}
void app_main(void)
{
    nvs_flash_init();

    get_gateway_id(g_gwid);
    ESP_LOGI(TAG, "GWID = %02X:%02X:%02X:%02X:%02X:%02X",
             g_gwid[0], g_gwid[1], g_gwid[2], g_gwid[3], g_gwid[4], g_gwid[5]);

    // IPC
    g_q_resp = xQueueCreate(RESP_QUEUE_LEN, sizeof(lora_header_t));
    g_lora_mutex = xSemaphoreCreateMutex();
    configASSERT(g_q_resp && g_lora_mutex);

    xTaskCreate(rx_task, "rx_task", 4096, NULL, 5, NULL);
    xTaskCreate(resp_task, "resp_task", 4096, NULL, 4, NULL);
}