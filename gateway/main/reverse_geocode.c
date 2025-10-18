// reverse_geocode.c
#include "reverse_geocode.h"
#include <math.h>
#include <stddef.h>

#include "province_lut.h"

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

bool reverse_geocode_province(float lat, float lon, const char **province_name_out)
{
    if (!province_name_out)
        return false;

    if (lat < PROV_LAT_MIN || lon < PROV_LON_MIN)
    {
        *province_name_out = "UNKNOWN";
        return false;
    }
    int i = (int)lroundf((lat - PROV_LAT_MIN) / PROV_STEP);
    int j = (int)lroundf((lon - PROV_LON_MIN) / PROV_STEP);
    i = clampi(i, 0, PROV_H - 1);
    j = clampi(j, 0, PROV_W - 1);

    uint8_t pid = PROV_LUT[i][j];
    if (pid == 0)
    {
        *province_name_out = "UNKNOWN";
        return false;
    }

    *province_name_out = PROV_NAMES[pid];
    return true;
}
