#ifndef __WIFI_AP_H
#define __WIFI_AP_H

#include "esp_err.h"

esp_err_t start_ap_and_server(void);
esp_err_t stop_ap_and_server(void);

#endif
