#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <string.h>
#include "lora.h"
#include "esp_log.h"
#include <inttypes.h> // Thêm thư viện để sử dụng PRIu32
#define LED_GPIO 2
#define mac_address "EC:64:C9:86:DC:EC"

#define I2C_PORT I2C_NUM_0

#define LORA_FREQUENCY 433E6

#define MLX90614_READ_DURATION 5000
#define MAX30102_READ_DURATION 30000
void blink_task(void *pvParameter)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(500)); // bật 500ms
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(500)); // tắt 500ms
    }
}
static const char *TAG = "SENSOR_TASK";

void send_lora_data(float temperature, float heart_rate, float spo2)
{
    char buf[100];

    int len = snprintf(buf, sizeof(buf), "MAC: %s, Temp: %.2f, HR: %.2f, SpO2: %.2f", mac_address, temperature, heart_rate, spo2);

    lora_send_packet((uint8_t *)buf, len);
    ESP_LOGI(TAG, "Sent: %s", buf);
}

void sensor_task(void *pvParameters)
{

    float temperature_c = 20;
    float heart_rate = 0;
    float spo2 = 0;
    uint32_t ir_data = 0;
    uint32_t red_data = 0;
    bool flag = false;

    lora_init();
    lora_set_frequency(LORA_FREQUENCY);
    lora_set_tx_power(17);
    lora_set_spreading_factor(12);
    lora_set_bandwidth(125E3);
    lora_enable_crc();

    while (1)
    {

        uint32_t start_time = xTaskGetTickCount();
        while (xTaskGetTickCount() - start_time < 300)
        {
            send_lora_data(temperature_c, heart_rate, spo2);
            ESP_LOGI(TAG, "IR Data: %lu", ir_data);

            vTaskDelay(pdMS_TO_TICKS(600));
        }
    }
}
void app_main(void)
{
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 4, NULL);
}