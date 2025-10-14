#ifndef __WIFI_H__
#define __WIFI_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_err.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_GOT_IP_BIT BIT1

// NVS keys
#define NVS_NS_WIFI "wifi"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"

typedef enum
{
    CONNECTED = 0,
    GOT_IP,
    DISCONNECTED
} wifi_state_t;

EventGroupHandle_t wifi_get_event_group(void);

// Load/save Wi-Fi
bool wifi_nvs_load(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len);
void wifi_nvs_save(const char *ssid, const char *pass);

bool wifi_sta_start_and_wait_ip(uint32_t timeout_ms);

void wifi_nvs_reset_wifi(void);
void wifi_sta_stop(void);
#endif
