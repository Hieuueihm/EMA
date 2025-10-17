#ifndef __TIMESYNC_H
#define __TIMESYNC_H
#include "esp_log.h"
#include "esp_sntp.h" // IDF 4.x/5.x: <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
bool ntp_start(void);
bool time_is_synced(void);
int64_t now_ms(void);
#endif