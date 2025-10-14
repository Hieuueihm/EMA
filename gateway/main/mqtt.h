#ifndef __MQTT_H
#define __MQTT_H

#include "mqtt_client.h"
#include "esp_log.h"
#include <stdbool.h>
#include "cJSON.h"
void mqtt_init(void);
bool mqtt_start(uint32_t timeout_ms);
void mqtt_stop(void);
bool mqtt_is_connected(void);

bool gw_connect_device(const char *dev);
bool gw_disconnect_device(const char *dev);
bool gw_publish_telemetry(const char *dev, cJSON *values_obj);
bool gw_publish_telemetry_ts(const char *dev, cJSON *values_obj);
bool gw_publish_attributes(const char *dev, cJSON *attrs_obj);

#endif