#ifndef __SX1278_H
#define __SX1278_H

#include "common.h"
#include "stm32f1xx_hal_spi.h"


#define LORA_RST_PIN   0
#define LORA_RST_PORT   GPIOB  

#define LORA_CS_PIN     4
#define LORA_CS_PORT    GPIOA



#define REG_FIFO                       0x00
#define REG_OP_MODE                    0x01
#define REG_FRF_MSB                    0x06
#define REG_FRF_MID                    0x07
#define REG_FRF_LSB                    0x08
#define REG_PA_CONFIG                  0x09
#define REG_LNA                        0x0c
#define REG_FIFO_ADDR_PTR              0x0d
#define REG_FIFO_TX_BASE_ADDR          0x0e
#define REG_FIFO_RX_BASE_ADDR          0x0f
#define REG_FIFO_RX_CURRENT_ADDR       0x10
#define REG_IRQ_FLAGS                  0x12
#define REG_RX_NB_BYTES                0x13
#define REG_PKT_SNR_VALUE              0x19
#define REG_PKT_RSSI_VALUE             0x1a
#define REG_MODEM_LORA_1             0x1d
#define REG_MODEM_LORA_2             0x1e
#define REG_PREAMBLE_MSB               0x20
#define REG_PREAMBLE_LSB               0x21
#define REG_PAYLOAD_LENGTH             0x22
#define REG_MODEM_LORA_3             0x26
#define REG_RSSI_WIDEBAND              0x2c
#define REG_DETECTION_OPTIMIZE         0x31
#define REG_DETECTION_THRESHOLD        0x37
#define REG_SYNC_WORD                  0x39
#define REG_DIO_MAPPING_1              0x40
#define REG_VERSION                    0x42


#define MODE_LONG_RANGE_MODE           0x80
#define MODE_SLEEP                     0x00
#define MODE_STDBY                     0x01
#define MODE_TX                        0x03
#define MODE_RX_CONTINUOUS             0x05
#define MODE_RX_SINGLE                 0x06



#define PA_BOOST                       0x80


#define IRQ_TX_DONE_MASK               0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_RX_DONE_MASK               0x40

#define PA_OUTPUT_RFO_PIN              0
#define PA_OUTPUT_PA_BOOST_PIN         1

#define TIMEOUT_RESET                  100



typedef struct SX1278_t LoRa;

struct SX1278_t{
    uint32_t freq;
    uint32_t implicit;
    struct {
        void (*lora_write_reg)(uint8_t reg, uint32_t val);
        uint32_t (*lora_read_reg)(uint8_t reg);
        void (*lora_reset)(void);
        void (*lora_explicit_header_mode)(LoRa *lr);
        void (*lora_implicit_header_mode)(LoRa *lr, uint32_t size);
        void (*lora_idle)(LoRa *lr);
        void (*lora_sleep)(LoRa *lr);
        void (*lora_receive)(LoRa *lr);
        void (*lora_set_tx_power)(LoRa *lr,uint8_t level);
        void (*lora_set_frequency)(LoRa *lr, uint32_t frequency);
        void (*lora_set_spreading_factor)(LoRa *lr, uint8_t sf);
        void (*lora_set_bandwidth)(LoRa *lr, uint32_t sbw);
        void (*lora_set_coding_rate)(LoRa *lr, uint8_t denominator);
        void (*lora_set_preamble_length)(LoRa *lr, uint8_t length);
        void (*lora_set_sync_word)(LoRa *lr, uint32_t sw);
        void (*lora_enable_crc)(LoRa *lr);
        void (*lora_disable_crc)(LoRa *lr);
        int (*lora_init)(LoRa *lr);
        void (*lora_send_packet)(LoRa *lr, uint8_t *buf, uint32_t size);
        uint32_t (*lora_receive_packet)(LoRa *lr, uint8_t *buf, uint32_t size);
        Status_e (*lora_received)(LoRa *lr);
        uint32_t (*lora_packet_rssi)(LoRa *lr);
        float (*lora_packet_snr)(LoRa *lr);
        void (*lora_dump_registers)(LoRa *lr);
    } api;

};

LoRa SX1278_Init(void);


#endif