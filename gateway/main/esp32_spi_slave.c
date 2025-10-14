#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_SCLK 18
#define PIN_NUM_CS 5

#define BUFFER_SIZE 32

static const char *TAG = "SPI_SLAVE";

void app_main(void)
{
    esp_err_t ret;

    // 1. Cấu hình SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUFFER_SIZE};

    // 2. Cấu hình SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .mode = 0, // SPI mode 0
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 12, // số lượng transaction có thể xếp hàng
        .flags = 0,
        .post_setup_cb = NULL,
        .post_trans_cb = NULL};

    // 3. Init SPI slave
    ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init SPI slave");
        return;
    }

    ESP_LOGI(TAG, "SPI Slave initialized.");

    uint8_t recvbuf[BUFFER_SIZE];
    memset(recvbuf, 0, sizeof(recvbuf));

    while (1)
    {
        spi_slave_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 2 * 8; // length tính theo bit
        t.rx_buffer = recvbuf;

        // Chờ master gửi dữ liệu
        ret = spi_slave_transmit(VSPI_HOST, &t, portMAX_DELAY);
        if (ret == ESP_OK)
        {
            ESP_LOG_BUFFER_HEX(TAG, recvbuf, 2);
        }
        else
        {
            ESP_LOGE(TAG, "SPI receive failed");
        }
    }
}
