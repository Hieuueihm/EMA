#include "timesync.h"

static const char *TAG = "TIME";

static bool s_time_synced = false;

static void time_sync_notification_cb(struct timeval *tv)
{
    s_time_synced = true;
    ESP_LOGI(TAG, "Time synchronized");
}

bool ntp_start(void)
{
    s_time_synced = false;

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    setenv("TZ", "ICT-7", 1);

    tzset();

    // Chờ đồng bộ tối đa ~10s
    for (int i = 0; i < 100 && !s_time_synced; ++i)
    {
        struct tm tm_now = {0};
        time_t now = time(NULL);
        localtime_r(&now, &tm_now);
        if (tm_now.tm_year + 1900 > 2020)
        {
            s_time_synced = true;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    time_t now = time(NULL);
    struct tm tminfo;
    localtime_r(&now, &tminfo);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &tminfo);
    ESP_LOGI(TAG, "Current time: %s (synced=%d)", buf, s_time_synced);
    return false;
}

bool time_is_synced(void)
{
    return s_time_synced;
}

int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (tv.tv_usec / 1000);
}
