#include "lora_packet.h"


void lora_pkt_fill_header(lora_header_t *hdr,
                          lora_msg_type_t type,
                          uint16_t device_id,
                          const uint8_t gateway_id[6],
                          uint16_t seq16, uint8_t ack){
    if (!hdr) return;
    hdr->msg_type      = (uint8_t)type;
    hdr->device_id = device_id;
    safe_memcpy(hdr->gateway_id, gateway_id ? gateway_id : ZERO_GWID, 6);
    hdr->seq16      = seq16;
    hdr->ack = ack;
}

static uint16_t emit_header(uint8_t *out, uint16_t out_cap, const lora_header_t *h)
{
    if (!out || !h || out_cap < LORA_HEADER_LEN) return 0;

    out[0] = h->msg_type;                
    write_u16_be(&out[1], h->device_id); 
    memcpy(&out[3], h->gateway_id, 6);   
    write_u16_be(&out[9], h->seq16);    
    write_u16_be(&out[11], h->ack); 
    return LORA_HEADER_LEN;              
}
uint16_t lora_pkt_build_empty(uint8_t *out, uint16_t out_cap,
                              lora_msg_type_t type,
                              uint16_t device_id,
                              const uint8_t gateway_id[6],
                              uint16_t seq16, uint8_t ack )
{
    lora_header_t hdr;
    lora_pkt_fill_header(&hdr, type, device_id, gateway_id, seq16, ack);
    return emit_header(out, out_cap, &hdr); 
}



uint16_t lora_pkt_build_hello(uint8_t *out, uint16_t out_cap,
                                            uint16_t device_id,
                                            const uint8_t gateway_id[6],
                                            uint16_t seq16, uint8_t ack)
{
    return lora_pkt_build_empty(out, out_cap, MSG_HELLO, device_id, gateway_id, seq16,ack);
}

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

uint16_t build_data_packet(uint8_t *out, uint16_t out_cap,
                                  uint16_t device_id,
                                  const uint8_t gateway_id[6],
                                  uint16_t seq16,
                                  const uint8_t *payload, uint16_t payload_len)
{
    if (!out || !gateway_id) return 0;
    if (out_cap < (uint16_t)(LORA_HEADER_LEN + payload_len)) return 0;

    out[0] = MSG_DATA_FNODE;
    write_u16_be(&out[1], device_id);
    memcpy(&out[3], gateway_id, 6);
    write_u16_be(&out[9], seq16);
    out[11] = 0; 

    if (payload_len) memcpy(&out[LORA_HEADER_LEN], payload, payload_len);
    return (uint16_t)(LORA_HEADER_LEN + payload_len);
}

