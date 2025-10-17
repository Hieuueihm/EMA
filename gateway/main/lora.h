#ifndef __LORA_H__
#define __LORA_H__

#include <stdbool.h>
#define LORA_CS_GPIO 18   // Pin số 18 cho CS
#define LORA_RST_GPIO 23  // Pin số 23 cho RST
#define LORA_MISO_GPIO 19 // Pin số 19 cho MISO
#define LORA_MOSI_GPIO 27 // Pin số 27 cho MOSI
#define LORA_SCK_GPIO 5   // Pin số 5 cho SCK
#define LORA_SEND_PACKET_TIMEOUT 5000

void lora_reset(void);
void lora_explicit_header_mode(void);
void lora_implicit_header_mode(int size);
void lora_idle(void);
void lora_sleep(void);
void lora_receive(void);
void lora_set_tx_power(int level);
void lora_set_frequency(long frequency);
void lora_set_spreading_factor(int sf);
void lora_set_bandwidth(long sbw);
void lora_set_coding_rate(int denominator);
void lora_set_preamble_length(long length);
void lora_set_sync_word(int sw);
void lora_enable_crc(void);
void lora_disable_crc(void);
int lora_init(void);
bool lora_send_packet(uint8_t *buf, int size);
int lora_receive_packet(uint8_t *buf, int size);
int lora_received(void);
int lora_packet_rssi(void);
float lora_packet_snr(void);
void lora_close(void);
void lora_dump_registers(void);

#endif