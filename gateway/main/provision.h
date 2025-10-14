#ifndef __PROVISION_H
#define __PROVISION_H

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#define MAX_DEVICES 128

#define MSG_HELLO 0x01
#define MSG_OFFER 0x02
#define MSG_REQUEST 0x03
#define MSG_ACK 0x04
#define MSG_DATA 0x05
typedef enum
{
    DEV_UNKNOWN = 0,
    DEV_REQUESTED = 1,
    DEV_PROVISIONED = 2,
    DEV_REVOKED = 3
} dev_state_t;
typedef struct
{
    uint8_t msg_type;
    uint8_t flags;
    uint16_t seq16; // hand-shake message sequence number
} header_t;

typedef struct
{
    header_t h;
    uint8_t current_uid[8];
    uint16_t nonce;
} hello_t;

typedef struct
{
    header_t h;
    uint8_t gw_id[6];
    uint8_t proposed_uid[8];
    uint16_t echo_nonce;
} offer_t;

typedef struct
{
    header_t h;
    uint8_t gw_id[6];
    uint8_t proposed_uid[8];
    uint16_t echo_nonce;
} request_t;

typedef struct
{
    header_t h;
    uint8_t assigned_uid[8];
    uint8_t gw_id[6];
    uint8_t status; // 0=OK, 1=DENY/EXPIRED
} ack_t;

typedef struct
{
    header_t h;
    uint8_t uid[8];
    uint32_t seq32;
    uint8_t len;
    uint8_t payload[64];
} data_t;
typedef struct __attribute__((packed))
{
    uint8_t uid[8];
    dev_state_t state;
    uint32_t last_seq;
    int16_t last_rssi;
    int8_t last_snr;
    uint32_t last_seen_ms;
    uint8_t in_use;
} dev_entry_t;
void provision_init(void);

void provision_get_gw_id(uint8_t out[6]);
void provision_set_gw_id(const uint8_t in[6]);

int provision_find_uid(const uint8_t uid[8]);
int provision_alloc_uid(const uint8_t uid[8]);
bool provision_db_save(void);

void provision_uid_generate(uint8_t out_uid[8]);

void provision_fill_header(header_t *h, uint8_t msg_type, uint8_t flags, uint16_t seq16);

offer_t provision_make_offer(const uint8_t proposed_uid[8], const uint8_t gw_id[6], uint16_t echo_nonce, uint16_t seq16);
ack_t provision_make_ack(const uint8_t assigned_uid[8], const uint8_t gw_id[6], uint8_t status, uint16_t seq16);

void provision_fill_offer(offer_t *o, const uint8_t proposed_uid[8], const uint8_t gw_id[6], uint16_t echo_nonce, uint16_t seq16);
void provision_fill_ack(ack_t *a, const uint8_t assigned_uid[8], const uint8_t gw_id[6], uint8_t status, uint16_t seq16);

typedef enum
{
    PROV_ACT_NONE = 0,
    PROV_ACT_SEND_OFFER,
    PROV_ACT_SEND_ACK
} prov_action_t;

typedef struct
{
    prov_action_t act;
    union
    {
        offer_t offer;
        ack_t ack;
    } frame;
} prov_result_t;

bool provision_handle_hello(const void *buf, size_t len, int16_t rssi, int8_t snr, prov_result_t *out);

bool provision_handle_request(const void *buf, size_t len, prov_result_t *out);

bool provision_handle_data(const void *buf, size_t len, int16_t rssi, int8_t snr, bool *accepted);
#endif