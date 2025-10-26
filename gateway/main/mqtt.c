#include "mqtt.h"
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include "timesync.h"
#include "lora_packet.h"
#include "main.h"
esp_mqtt_client_handle_t mqttClient = NULL;
#define TAG "MQTT"

#define MQTT_BROKER_URI "mqtt://demo.thingsboard.io:1883"
#define ACCESS_TOKEN "scb2rVM0tnzODm1Vqc6x"

static esp_mqtt_client_handle_t s_client = NULL;
static EventGroupHandle_t s_mqtt_evt = NULL;
#define MQTT_CONNECTED_BIT BIT0

static uint16_t seq16_random(void)
{
    uint32_t tick = (uint32_t)xTaskGetTickCount();
    tick ^= (tick << 11);
    tick ^= (tick >> 7);
    tick ^= (tick << 3);
    return (uint16_t)(tick & 0xFFFF);
}

static bool lora_enqueue_node_ctr(uint16_t dev_id)
{
    if (g_q_resp == NULL)
    {
        ESP_LOGE("APP", "RESP queue not ready");
        return false;
    }
    lora_header_t ctr = {0};
    ctr.msg_type = MSG_NODE_CTR;
    ctr.device_id = dev_id;
    ctr.seq16 = seq16_random();
    memcpy(ctr.gateway_id, g_gwid, 6);
    ctr.ack = 1;

    if (xQueueSend(g_q_resp, &ctr, 0) != pdTRUE)
    {
        ESP_LOGW("APP", "RESP queue full (NODE_CTR) dev=0x%04X", dev_id);
        return false;
    }
    ESP_LOGI("APP", "NODE_CTR enq dev=0x%04X seq=0x%04X", dev_id, ctr.seq16);
    return true;
}
static bool devname_to_devid(const char *name, uint16_t *out)
{
    if (!name || !out)
        return false;
    const char *p = name;
    if (strncmp(name, "NODE_", 5) == 0)
        p = name + 5;
    unsigned x = 0;
    if (sscanf(p, "%x", &x) == 1 && x <= 0xFFFF)
    {
        *out = (uint16_t)x;
        return true;
    }
    return false;
}
static bool mqtt_wait_connected(uint32_t timeout_ms)
{
    if (!s_mqtt_evt)
        return false;
    EventBits_t bits = xEventGroupWaitBits(
        s_mqtt_evt, MQTT_CONNECTED_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

bool mqtt_is_connected(void)
{
    if (!s_mqtt_evt)
        return false;
    return (xEventGroupGetBits(s_mqtt_evt) & MQTT_CONNECTED_BIT) != 0;
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "CONNECTED");
        if (s_mqtt_evt)
            xEventGroupSetBits(s_mqtt_evt, MQTT_CONNECTED_BIT);
        esp_mqtt_client_subscribe(event->client, "v1/gateway/attributes", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "DISCONNECTED");
        if (s_mqtt_evt)
            xEventGroupClearBits(s_mqtt_evt, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "SUB msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "UNSUB msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "PUB msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "DATA topic=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA %.*s", event->data_len, event->data);

        if (strncmp(event->topic, "v1/gateway/attributes", event->topic_len) == 0)
        {
            char *buf = malloc(event->data_len + 1);
            memcpy(buf, event->data, event->data_len);
            buf[event->data_len] = 0;
            cJSON *root = cJSON_Parse(buf);
            if (root)
            {
                cJSON *dev = cJSON_GetObjectItemCaseSensitive(root, "device");
                uint16_t dev_id = 0;
                if (devname_to_devid(dev->valuestring, &dev_id))
                {
                    lora_enqueue_node_ctr(dev_id);
                }
                else
                {
                    ESP_LOGW(TAG, "Unknown device name: %s", dev->valuestring);
                }
            }
            cJSON_Delete(root);
            free(buf);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "ERROR type=0x%x", event->error_handle ? event->error_handle->error_type : -1);
        if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGW(TAG, "tls err=0x%x stack=0x%x errno=%d",
                     event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_tls_stack_err,
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        ESP_LOGD(TAG, "OTHER id=%d", event->event_id);
        break;
    }
}
void mqtt_init(void)
{
    if (!s_mqtt_evt)
        s_mqtt_evt = xEventGroupCreate();
    if (s_client)
    {
        ESP_LOGI(TAG, "mqtt already initialized");
        return;
    }

    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = ACCESS_TOKEN,
    };
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "mqtt initialized");
}

bool mqtt_start(uint32_t timeout_ms)
{
    if (!s_client)
        mqtt_init();
    if (mqtt_is_connected())
        return true;

    esp_err_t e = esp_mqtt_client_start(s_client); // idempotent
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(e));
        return false;
    }
    bool ok = mqtt_wait_connected(timeout_ms);
    if (!ok)
        ESP_LOGW(TAG, "wait CONNECTED timeout");
    ESP_LOGI(TAG, "mqtt started");
    return ok;
}

void mqtt_stop(void)
{
    if (!s_client)
        return;
    esp_mqtt_client_stop(s_client);
    if (s_mqtt_evt)
        xEventGroupClearBits(s_mqtt_evt, MQTT_CONNECTED_BIT);
    ESP_LOGI(TAG, "mqtt stopped");
}

bool gw_connect_device(const char *dev)
{
    if (!mqtt_is_connected())
        return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", dev);
    char *payload = cJSON_PrintUnformatted(root);

    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/connect", payload, 0, 1, 0);
    ESP_LOGI(TAG, "CONNECT %s msg_id=%d", dev, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}

bool gw_disconnect_device(const char *dev)
{
    if (!mqtt_is_connected())
        return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", dev);
    char *payload = cJSON_PrintUnformatted(root);

    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/disconnect", payload, 0, 1, 0);
    ESP_LOGI(TAG, "DISCONNECT %s msg_id=%d", dev, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}

bool gw_publish_telemetry_ts(const char *dev, int64_t ts, cJSON *values_obj)
{
    if (!mqtt_is_connected())
    {
        ESP_LOGW(TAG, "Not connected");
        cJSON_Delete(values_obj);
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    cJSON *pt = cJSON_CreateObject();

    if (ts > 0)
    {
        cJSON_AddNumberToObject(pt, "ts", ts);
    }
    cJSON_AddItemToObject(pt, "values", values_obj);
    cJSON_AddItemToArray(arr, pt);
    cJSON_AddItemToObject(root, dev, arr);

    char *payload = cJSON_PrintUnformatted(root);
    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/telemetry", payload, 0, 1, 0);
    // ESP_LOGI(TAG, "TX telemetry %s: %s (msg_id=%d)", dev, payload, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}

bool gw_publish_telemetry(const char *dev, cJSON *values_obj)
{
    if (time_is_synced())
    {
        return gw_publish_telemetry_ts(dev, now_ms(), values_obj);
    }
    else
    {
        return gw_publish_telemetry_ts(dev, 0, values_obj);
    }
}

bool gw_publish_attributes(const char *dev, cJSON *attrs_obj)
{
    if (!mqtt_is_connected())
    {
        cJSON_Delete(attrs_obj);
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, dev, attrs_obj);

    char *payload = cJSON_PrintUnformatted(root);
    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/attributes", payload, 0, 1, 0);
    // ESP_LOGD(TAG, "TX attributes %s: %s (msg_id=%d)", dev, payload, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}