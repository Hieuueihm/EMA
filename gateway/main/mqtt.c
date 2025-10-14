#include "mqtt.h"
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

esp_mqtt_client_handle_t mqttClient = NULL;
#define TAG "MQTT"

#define MQTT_BROKER_URI "mqtt://demo.thingsboard.io:1883"
#define ACCESS_TOKEN "scb2rVM0tnzODm1Vqc6x"

static esp_mqtt_client_handle_t s_client = NULL;
static EventGroupHandle_t s_mqtt_evt = NULL;
#define MQTT_CONNECTED_BIT BIT0

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
        esp_mqtt_client_subscribe(event->client, "v1/gateway/rpc", 1);
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
    if (!mqtt_start(5000))
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
    if (!mqtt_start(3000))
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

bool gw_publish_telemetry_ts(const char *dev, cJSON *values_obj)
{
    if (!mqtt_start(5000))
    {
        cJSON_Delete(values_obj);
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    cJSON *pt = cJSON_CreateObject();

    cJSON_AddItemToObject(pt, "values", values_obj); // transfer ownership
    cJSON_AddItemToArray(arr, pt);
    cJSON_AddItemToObject(root, dev, arr);

    char *payload = cJSON_PrintUnformatted(root);
    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/telemetry", payload, 0, 1, 0);
    ESP_LOGD(TAG, "TX telemetry %s: %s (msg_id=%d)", dev, payload, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}

bool gw_publish_telemetry(const char *dev, cJSON *values_obj)
{
    return gw_publish_telemetry_ts(dev, values_obj);
}

bool gw_publish_attributes(const char *dev, cJSON *attrs_obj)
{
    if (!mqtt_start(5000))
    {
        cJSON_Delete(attrs_obj);
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, dev, attrs_obj);

    char *payload = cJSON_PrintUnformatted(root);
    int msg_id = esp_mqtt_client_publish(s_client, "v1/gateway/attributes", payload, 0, 1, 0);
    ESP_LOGD(TAG, "TX attributes %s: %s (msg_id=%d)", dev, payload, msg_id);

    cJSON_free(payload);
    cJSON_Delete(root);
    return msg_id != -1;
}