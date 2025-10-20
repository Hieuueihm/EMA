#include "lora_packet.h"

bool lora_parse_header(const uint8_t *in, uint16_t in_len, lora_header_t *out)
{
    if (!in || !out || in_len < LORA_HEADER_LEN)
        return false;

    out->msg_type = in[0];
    out->device_id = read_u16_be(&in[1]);
    memcpy(out->gateway_id, &in[3], 6);
    out->seq16 = read_u16_be(&in[9]);
    out->ack = in[11];

    return true;
}
uint16_t lora_write_header_only(uint8_t *out, uint16_t cap, const lora_header_t *h)
{
    if (!out || !h || cap < LORA_HEADER_LEN)
        return 0;
    out[0] = h->msg_type;
    write_u16_be(&out[1], h->device_id);
    memcpy(&out[3], h->gateway_id, 6);
    write_u16_be(&out[9], h->seq16);
    out[11] = h->ack;
    return LORA_HEADER_LEN;
}