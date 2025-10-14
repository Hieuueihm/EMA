#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_SCLK 18
#define PIN_NUM_CS 5

static const char *TAG = "SPI_MASTER";
spi_device_handle_t spi;

void spi_master_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32, // max bytes
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
        .mode = 0,                         // CPOL=0, CPHA=0
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 3,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 0));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));

    ESP_LOGI(TAG, "SPI Master initialized");
}
void spi_master_send(uint8_t *data, size_t len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = len * 8; // bits
    t.tx_buffer = data;

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t)); // blocking
}
void app_main(void)
{
    spi_master_init();

    uint8_t txData[4] = {0x11, 0x22, 0x33, 0x44};

    while (1)
    {
        spi_master_send(txData, sizeof(txData));
        ESP_LOGI(TAG, "Master sent: %02X %02X %02X %02X",
                 txData[0], txData[1], txData[2], txData[3]);

        // Update dữ liệu nếu muốn
        txData[0]++;
        txData[1]++;
        txData[2]++;
        txData[3]++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
