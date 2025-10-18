#include "reverse_geocode.h"
#include "esp_log.h"
void handle_gps(float lat, float lon)
{
    const char *prov = NULL;
    bool ok = reverse_geocode_province(lat, lon, &prov);
    ESP_LOGI("GEO", "lat=%.6f lon=%.6f -> %s (%s)",
             lat, lon, prov, ok ? "match" : "fallback");
}

void app_main(void)
{
    handle_gps(10.762622, 106.660172);
    handle_gps(21.0278, 105.8342);
}