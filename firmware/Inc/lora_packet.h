#ifndef __LORA_PACKET_H
#define __LORA_PACKET_H

#include "common.h"
#define LORA_PKT_MAX_TOTAL_LEN   255 
#define LORA_HEADER_LEN            12 // 1 bytes msgtype + 2 bytes dev_id + 6 bytes gateway id + 2 bytes seq16 + ack
#define LORA_MAX_PAYLOAD_LEN     (LORA_PKT_MAX_TOTAL_LEN - LORA_HDR_LEN)
static const uint8_t ZERO_GWID[6] = {0,0,0,0,0,0};



typedef enum
{
    MSG_HELLO = 0x01,
    MSG_HELLO_RESP = 0x02,
    MSG_DATA_FNODE = 0x03,
    MSG_DATA_ACK = 0x04,
    MSG_NODE_CTR = 0x05,
    MSG_CTR_ACK = 0x06
} lora_msg_type_t;


typedef enum {
    HELLO_OK       = 0x00,
    HELLO_REJECT   = 0x01,
} lora_hello_status_t;

typedef struct {
    uint8_t  msg_type;    
    uint16_t device_id; 
    uint8_t  gateway_id[6];
    uint16_t seq16; 
    uint8_t ack;
} lora_header_t;
typedef struct {
    float    temperature;   
    float    humidity;      
    float    co_ppm;       
    float    uvi;           
    uint16_t pm25;  
    uint16_t pm10;  
    uint8_t buzzer;
    float latitude;    
    float longitude; 
} lora_payload_sensor_t;

// helpers 


uint16_t lora_write_header_only(uint8_t *out, uint16_t out_cap, const lora_header_t *h);


uint16_t lora_pkt_build_empty(uint8_t *out, uint16_t out_cap,
                              lora_msg_type_t type,
                              uint16_t device_id,
                              const uint8_t gateway_id[6],
                              uint16_t seq16, uint8_t ack);

uint16_t lora_pkt_build_hello(uint8_t *out, uint16_t out_cap,
                                            uint16_t device_id,
                                            const uint8_t gateway_id[6],
                                            uint16_t seq16, uint8_t ack);

bool lora_parse_header(const uint8_t *in, uint16_t in_len, lora_header_t *out);
uint16_t build_data_packet(uint8_t *out, uint16_t out_cap,
                           uint16_t device_id,
                           const uint8_t gateway_id[6],
                           uint16_t seq16,
                           const uint8_t *payload, uint16_t payload_len);



static void safe_memcpy(void *d, const void *s, size_t n){ if(d&&s&&n) memcpy(d,s,n); }


static uint16_t read_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_u16_be(uint8_t *p, uint16_t v_host)
{
    p[0] = (uint8_t)(v_host >> 8);   
    p[1] = (uint8_t)(v_host & 0xFF); 
}

static void write_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v & 0xFF);
}

static void write_f32_be(uint8_t *p, float f)
{
    union
    {
        float f;
        uint32_t u;
    } u;
    u.f = f;
    write_u32_be(p, u.u);
}
#endif