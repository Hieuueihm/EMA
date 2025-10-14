#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_mac.h"

#include "wifi_ap.h"
#include "wifi_sta.h"

static const char *TAG = "WIFI_AP";

/* ====== EMBEDDED HTML (linker symbols) ====== */
extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];

/* ====== STATE ====== */
static bool s_ap_running = false;
static httpd_handle_t s_http = NULL;

/* ====== UTIL ====== */
static void url_decode(char *dst, const char *src)
{
    const char *p = src;
    char *o = dst;
    while (*p)
    {
        if (*p == '+')
        {
            *o++ = ' ';
            p++;
        }
        else if (*p == '%' && p[1] && p[2])
        {
            char hex[3] = {p[1], p[2], 0};
            *o++ = (char)strtol(hex, NULL, 16);
            p += 3;
        }
        else
        {
            *o++ = *p++;
        }
    }
    *o = '\0';
}

/* ====== HTTP Handlers ====== */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint8_t *start = _binary_index_html_start;
    const uint8_t *end = _binary_index_html_end;
    size_t len = (size_t)(end - start);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)start, len);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[384];
    int total = 0;
    int remaining = req->content_len;
    while (remaining > 0 && total < (int)sizeof(buf) - 1)
    {
        int r = httpd_req_recv(req, buf + total,
                               remaining > (int)sizeof(buf) - 1 - total ? (int)sizeof(buf) - 1 - total : remaining);
        if (r <= 0)
            return ESP_FAIL;
        total += r;
        remaining -= r;
    }
    buf[total] = 0;

    char ssid[64] = {0}, pass[96] = {0};
    char *p_ssid = strstr(buf, "ssid=");
    char *p_pass = strstr(buf, "pass=");
    if (p_ssid)
    {
        p_ssid += 5;
        char *amp = strchr(p_ssid, '&');
        int len = amp ? (int)(amp - p_ssid) : (int)strlen(p_ssid);
        char tmp[96] = {0};
        memcpy(tmp, p_ssid, len > 95 ? 95 : len);
        url_decode(ssid, tmp);
    }
    if (p_pass)
    {
        p_pass += 5;
        char tmp[128] = {0};
        strncpy(tmp, p_pass, sizeof(tmp) - 1);
        url_decode(pass, tmp);
    }
    if (ssid[0] == '\0')
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_OK;
    }

    wifi_nvs_save(ssid, pass);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<html><body><h3>Saved! Rebooting...</h3></body></html>");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    if (s_http)
    {
        return ESP_OK; // already running
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.ctrl_port = 32768;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &root);

    httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &save);

    s_http = server;
    ESP_LOGI(TAG, "HTTP server started: http://192.168.4.1/");
    return ESP_OK;
}

static void stop_http_server(void)
{
    if (!s_http)
        return;
    httpd_handle_t h = s_http;
    s_http = NULL;
    httpd_stop(h);
}

esp_err_t start_ap_and_server(void)
{
    if (s_ap_running)
    {
        ESP_LOGI(TAG, "AP already running");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Switching to AP + Portal");

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif)
    {
        ap_netif = esp_netif_create_default_wifi_ap();
        if (!ap_netif)
        {
            ESP_LOGE(TAG, "Failed to create default WIFI AP netif");
            return ESP_FAIL;
        }
    }

    // Cấu hình SSID theo MAC
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "GW-Setup-%02X%02X", mac[4], mac[5]);

    wifi_config_t ap = {0};
    strncpy((char *)ap.ap.ssid, ap_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(ap_ssid);
    strncpy((char *)ap.ap.password, "12345678", sizeof(ap.ap.password));
    ap.ap.channel = 6;
    ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap.ap.max_connection = 4;
    if (strlen("12345678") == 0)
        ap.ap.authmode = WIFI_AUTH_OPEN;

    // Chuyển sang AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "AP: SSID='%s' PASS='%s'", ap_ssid, "12345678");
    ESP_LOGI(TAG, "DHCP server IP: 192.168.4.1");

    ESP_ERROR_CHECK(start_http_server());

    s_ap_running = true;
    return ESP_OK;
}

esp_err_t stop_ap_and_server(void)
{
    if (!s_ap_running)
    {
        stop_http_server();
        return ESP_OK;
    }

    stop_http_server();

    esp_wifi_stop();

    s_ap_running = false;
    ESP_LOGI(TAG, "AP portal stopped");
    return ESP_OK;
}
