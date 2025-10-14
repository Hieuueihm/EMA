#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lora.h"

#define TAG "NODE_HELLO"

#define MSG_HELLO 0x01
#define MSG_OFFER 0x02
#include "nvs_flash.h"
#include "nvs.h"

static void dump_devdb(void)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open("gwdb", NVS_READONLY, &h);
    if (e != ESP_OK)
    {
        ESP_LOGW(TAG, "No gwdb found");
        return;
    }
    size_t size = 0;
    e = nvs_get_blob(h, "devdb", NULL, &size);
    if (e != ESP_OK || size == 0)
    {
        ESP_LOGW(TAG, "devdb empty");
        nvs_close(h);
        return;
    }

    uint8_t *buf = malloc(size);
    if (!buf)
    {
        nvs_close(h);
        return;
    }
    e = nvs_get_blob(h, "devdb", buf, &size);
    nvs_close(h);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "read devdb failed %d", e);
        free(buf);
        return;
    }

    // Parse array dev_entry_t đã định nghĩa trong provision.c
    int count = size / sizeof(dev_entry_t);
    dev_entry_t *db = (dev_entry_t *)buf;
    for (int i = 0; i < count; i++)
    {
        if (!db[i].in_use)
            continue;
        char uid_str[3 * 8] = {0};
        int off = 0;
        for (int j = 0; j < 8; j++)
            off += snprintf(uid_str+off, sizeof(uid_str)-off,
                            "%02X%s", db[i].uid[j], (j<7?\":\":\""));
        ESP_LOGI(TAG, "DB[%d]: UID=%s state=%d last_seq=%u last_seen=%u ms",
                 i, uid_str, db[i].state,
                 (unsigned)db[i].last_seq, (unsigned)db[i].last_seen_ms);
    }
    free(buf);
}

#pragma pack(push, 1)
typedef struct
{
    uint8_t msg_type; // = MSG_HELLO
    uint8_t flags;    // 0
    uint16_t seq16;   // random
} header_t;

// HELLO payload: UID hiện tại (0 nếu chưa được cấp) + nonce
typedef struct
{
    header_t h;
    uint8_t current_uid[8]; // all-zero để xin UID
    uint16_t nonce;         // random để gateway echo lại
} hello_t;

// OFFER (định nghĩa để parse ở node)
typedef struct
{
    uint8_t msg_type; // = MSG_OFFER
    uint8_t flags;    // 0
    uint16_t seq16;   // echo từ HELLO
    uint8_t gw_id[6];
    uint8_t proposed_uid[8];
    uint16_t echo_nonce;
} offer_t;
#pragma pack(pop)

/* ===== Utils ===== */
static inline uint16_t rnd16(void) { return (uint16_t)(rand() & 0xFFFF); }

static void hexdump(const char *tag, const uint8_t *p, int n)
{
    printf("%s (%dB): ", tag, n);
    for (int i = 0; i < n; i++)
        printf("%02X ", p[i]);
    printf("\n");
}

/* ===== Gửi HELLO (UID=0) và in OFFER nhận được ===== */
static void log_offer(const offer_t *o, int nbytes)
{
    char uid_str[3 * 8] = {0};
    int off = 0;
    for (int i = 0; i < 8; i++)
        off += snprintf(uid_str + off, sizeof(uid_str) - off,
                        "%02X%s", o->proposed_uid[i], (i < 7 ? ":" : ""));

    ESP_LOGI(TAG,
             "OFFER: seq16=0x%04X, echo_nonce=0x%04X, gw_id=%02X:%02X:%02X:%02X:%02X:%02X, "
             "proposed_uid=%s",
             o->seq16, o->echo_nonce,
             o->gw_id[0], o->gw_id[1], o->gw_id[2], o->gw_id[3], o->gw_id[4], o->gw_id[5],
             uid_str);

    hexdump("OFFER_RAW", (const uint8_t *)o, nbytes);
}

static void wait_and_log_offers(uint16_t expect_seq16, uint16_t expect_nonce, uint32_t window_ms)
{
    uint32_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(window_ms);
    uint8_t rx[256];

    ESP_LOGI(TAG, "Waiting for OFFERs (window %ums)...", (unsigned)window_ms);

    while ((int32_t)(xTaskGetTickCount() - deadline) < 0)
    {
        lora_receive();
        if (!lora_received())
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int n = lora_receive_packet(rx, sizeof(rx));
        if (n <= 0)
            continue;
        if ((size_t)n < sizeof(offer_t))
            continue;

        const offer_t *o = (const offer_t *)rx;
        if (o->msg_type != MSG_OFFER)
            continue; // không phải OFFER
        if (o->seq16 != expect_seq16)
            continue; // seq không khớp HELLO của mình
        if (o->echo_nonce != expect_nonce)
            continue; // nonce không khớp

        log_offer(o, n);
    }
}

static void send_hello_uid0_and_log_offers(void)
{
    // Build HELLO (UID=0)
    hello_t pkt = {0};
    uint16_t seq16 = rnd16();
    uint16_t nonce = rnd16();

    pkt.h.msg_type = MSG_HELLO;
    pkt.h.flags = 0;
    pkt.h.seq16 = seq16;
    memset(pkt.current_uid, 0x00, sizeof(pkt.current_uid)); // chưa có UID
    pkt.nonce = nonce;

    // Gửi
    int len = sizeof(pkt);
    lora_send_packet((const uint8_t *)&pkt, len);
    ESP_LOGI(TAG, "HELLO sent (len=%d, seq=0x%04X, nonce=0x%04X)", len, seq16, nonce);

    // Nghe và in các OFFER hợp lệ trong 800 ms
    wait_and_log_offers(seq16, nonce, 2000);
}

/* ===== Task chính: gửi HELLO liên tục để theo dõi OFFER ===== */
static void node_task(void *arg)
{
    (void)arg;

    while (1)
    {
        send_hello_uid0_and_log_offers();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1s, tránh WDT và cho gateway thời gian đáp
    }
}

void app_main(void)
{
    srand((unsigned)time(NULL));
    provision_init();
    dump_devdb();

    if (!lora_init())
    {
        ESP_LOGE(TAG, "LoRa initialization failed.");
        vTaskDelete(NULL);
        return;
    }

    // Cấu hình LoRa của bạn
    lora_set_frequency(433E6);
    lora_set_spreading_factor(12);
    lora_set_bandwidth(125E3);
    lora_enable_crc();

    xTaskCreatePinnedToCore(node_task, "node_hello", 4096, NULL, 8, NULL, 1);
}
