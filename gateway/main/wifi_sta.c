#include "wifi_sta.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "WIFI_STA";

/* ===== State ===== */
wifi_state_t wifi_state = DISCONNECTED;
static EventGroupHandle_t s_wifi_event_group = NULL;
static bool s_wifi_inited = false;
static bool s_handlers_registered = false;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_GOT_IP_BIT BIT1

EventGroupHandle_t wifi_get_event_group(void) { return s_wifi_event_group; }
extern volatile bool ap_mode;

bool wifi_nvs_load(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS_WIFI, NVS_READONLY, &h) != ESP_OK)
        return false;

    size_t a = ssid_len, b = pass_len;
    esp_err_t e1 = nvs_get_str(h, NVS_KEY_SSID, ssid_out, &a);
    if (e1 != ESP_OK)
    {
        nvs_close(h);
        return false;
    }

    esp_err_t e2 = nvs_get_str(h, NVS_KEY_PASS, pass_out, &b);
    if (e2 != ESP_OK)
        pass_out[0] = '\0';

    nvs_close(h);
    return true;
}

void wifi_nvs_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_set_str(h, NVS_KEY_SSID, ssid ? ssid : "");
        nvs_set_str(h, NVS_KEY_PASS, pass ? pass : "");
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG, "Saved SSID to NVS");
    }
    else
    {
        ESP_LOGE(TAG, "NVS open failed");
    }
}

void wifi_nvs_reset_wifi(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed (%s)", esp_err_to_name(err));
        return;
    }
    nvs_erase_key(h, NVS_KEY_SSID);
    nvs_erase_key(h, NVS_KEY_PASS);
    err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK)
        ESP_LOGW(TAG, "Wi-Fi creds erased.");
    else
        ESP_LOGE(TAG, "Commit failed (%s)", esp_err_to_name(err));
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        (void)esp_wifi_connect();
        ESP_LOGI(TAG, "STA started → connecting...");
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_state = CONNECTED;
        ESP_LOGI(TAG, "Associated to AP");
        if (s_wifi_event_group)
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_state = DISCONNECTED;
        ESP_LOGW(TAG, "Disconnected");
        if (s_wifi_event_group)
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);

        if (!ap_mode)
        {
            (void)esp_wifi_connect();
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        wifi_state = GOT_IP;
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        if (s_wifi_event_group)
            xEventGroupSetBits(s_wifi_event_group, WIFI_GOT_IP_BIT);
    }
}

static void wifi_sta_init_once(void)
{
    if (s_wifi_inited)
        return;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    s_wifi_inited = true;

    if (!s_wifi_event_group)
    {
        s_wifi_event_group = xEventGroupCreate();
    }

    if (!s_handlers_registered)
    {
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
        s_handlers_registered = true;
    }
}

bool wifi_sta_apply_saved_config(void)
{
    char ssid[64] = {0}, pass[96] = {0};
    if (!wifi_nvs_load(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        ESP_LOGW(TAG, "No saved Wi-Fi creds");
        return false;
    }

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_LOGI(TAG, "Applied SSID from NVS");
    return true;
}

bool wifi_sta_start_and_wait_ip(uint32_t timeout_ms)
{

    wifi_sta_init_once();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (s_wifi_event_group)
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_GOT_IP_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_GOT_IP_BIT)
    {
        ESP_LOGI(TAG, "GOT_IP");
        return true;
    }
    ESP_LOGW(TAG, "Timeout waiting IP");
    return false;
}

void wifi_sta_stop(void)
{
    if (!s_wifi_inited)
        return;

    (void)esp_wifi_disconnect();
    (void)esp_wifi_stop();

    wifi_state = DISCONNECTED;
    if (s_wifi_event_group)
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
}
