#include "fake_node.h"
#include "mqtt.h"

#define TAG "FAKE_NODE"
static float rnd_perturb(float r)
{
    // esp_random returns 32-bit random
    uint32_t v = esp_random();
    // normalize to [0,1)
    float u = (float)(v & 0xFFFFFF) / (float)0x1000000;
    return (u * 2.0f - 1.0f) * r;
}

void make_and_send_fake_for_node(const fake_node_t *n)
{
    // tạo object telemetry (giống bạn dùng trước đó)

    // giả lập các sensor với nhiễu nhỏ
    double lat = n->base_lat + (double)rnd_perturb(0.00005f); // ~ few meters
    double lon = n->base_lon + (double)rnd_perturb(0.00005f);

    // environment values (base + small random)
    float temp_base = 25.0f + (float)(n->device_id & 0x0F); // chỉ để khác nhau giữa nodes
    float hum_base = 60.0f;
    float uv_base = 3.0f;
    float pm25_base = 12.0f;
    float pm10_base = 30.0f;
    float co_base = 7.0f;

    float temp = temp_base + rnd_perturb(0.8f);
    float co = co_base + rnd_perturb(0.8f);
    float hum = hum_base + rnd_perturb(5.0f);
    float uv = uv_base + rnd_perturb(1.0f);
    float pm25 = pm25_base + rnd_perturb(6.0f);
    float pm10 = pm10_base + rnd_perturb(8.0f);
    uint8_t buzzer = n->buzzer;
    uint8_t dev_status = 0; // 0 OK, 1 WARN, 2 ERR
    // giả sử nếu pm25 > 50 → WARN
    if (pm25 > 50.0f)
        dev_status = 1;

    // add fields (tên phù hợp với ThingsBoard / gateway)
    cJSON *attrs = cJSON_CreateObject();
    if (attrs)
    {
        cJSON_AddNumberToObject(attrs, "device_id", n->device_id);
        cJSON_AddNumberToObject(attrs, "lat", lat);
        cJSON_AddNumberToObject(attrs, "lon", lon);
        cJSON_AddNumberToObject(attrs, "buzzer", buzzer);

        // gw_publish_attributes sẽ sở hữu attrs -> KHÔNG cJSON_Delete(attrs) ở đây
        if (!gw_publish_attributes(n->dev_name, attrs))
        {
            ESP_LOGW(TAG, "gw_publish_attributes failed for %s", n->dev_name);
            // nếu gw_publish_attributes trả về false mà KHÔNG nhận ownership,
            // thì bạn cần tự cJSON_Delete(attrs). Mặc định mình giả định callee nhận ownership.
        }
    }
    cJSON *vals = cJSON_CreateObject();
    if (!vals)
        return;

    // Nếu chỉ muốn lat/lon nằm ở attributes, THÌ ĐỪNG thêm 3 field này vào telemetry:
    // cJSON_AddNumberToObject(vals, "device_id", n->device_id);
    // cJSON_AddNumberToObject(vals, "lat", lat);
    // cJSON_AddNumberToObject(vals, "lon", lon);

    // phần telemetry còn lại:
    cJSON_AddNumberToObject(vals, "uv", (double)uv);
    cJSON_AddNumberToObject(vals, "pm25", (double)pm25);
    cJSON_AddNumberToObject(vals, "pm10", (double)pm10);
    cJSON_AddNumberToObject(vals, "temperature", (double)temp);
    cJSON_AddNumberToObject(vals, "humidity", (double)hum);
    cJSON_AddNumberToObject(vals, "co", (double)co);

    gw_connect_device(n->dev_name);

    if (mqtt_is_connected())
    {
        if (gw_publish_telemetry(n->dev_name, vals))
        {
            ESP_LOGI(TAG, "Published telemetry for %s (id=0x%04X): T=%.2f C, H=%.1f%%, PM2.5=%.1f",
                     n->dev_name, n->device_id, temp, hum, pm25);
        }
        else
        {
            ESP_LOGW(TAG, "gw_publish_telemetry failed for %s", n->dev_name);
            // KHÔNG xóa vals ở đây vì callee đã nhận ownership (AddItem...)
        }
    }
    else
    {
        ESP_LOGW(TAG, "MQTT not connected; cannot publish for %s", n->dev_name);
    }

    // cJSON_Delete(vals);
}