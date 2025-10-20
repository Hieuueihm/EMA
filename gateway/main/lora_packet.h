#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "string.h"
#define LORA_HEADER_LEN 12

typedef enum
{
    MSG_HELLO = 0x01,
    MSG_HELLO_RESP = 0x02,
    MSG_DATA_FNODE = 0x03,
    MSG_DATA_ACK = 0x04,
    MSG_NODE_CTR = 0x05,
} lora_msg_type_t;

typedef struct
{
    uint8_t msg_type;
    uint16_t device_id;
    uint8_t gateway_id[6];
    uint16_t seq16;
    uint8_t ack;
} lora_header_t;

bool lora_parse_header(const uint8_t *in, uint16_t in_len, lora_header_t *out);
uint16_t lora_write_header_only(uint8_t *out, uint16_t cap, const lora_header_t *h);

typedef struct
{
    float temperature;
    float humidity;
    float co_ppm;
    float uvi;
    uint16_t pm25;
    uint16_t pm10;
} sensor_payload_t;

#define SENSOR_PAYLOAD_MIN_LEN (4 * 3 + 1 + 2 * 2)

static inline uint16_t read_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static inline void write_u16_be(uint8_t *p, uint16_t v_host)
{
    p[0] = (uint8_t)(v_host >> 8);
    p[1] = (uint8_t)(v_host & 0xFF);
}

static inline void write_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v & 0xFF);
}

static inline void write_f32_be(uint8_t *p, float f)
{
    union
    {
        float f;
        uint32_t u;
    } u;
    u.f = f;
    write_u32_be(p, u.u);
}

static inline uint16_t rd_u16_be(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}
static inline uint32_t rd_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static inline float rd_f32_be(const uint8_t *p)
{
    union
    {
        uint32_t u;
        float f;
    } u;
    u.u = rd_u32_be(p);
    return u.f;
}