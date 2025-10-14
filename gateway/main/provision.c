#include "provision.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include <assert.h>
#include "esp_timer.h"

static inline uint32_t millis(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

static dev_entry_t g_db[MAX_DEVICES];
static uint8_t g_gw_id[6];
static uint8_t g_uid_counter[8];

static const char *state_str(dev_state_t s)
{
    switch (s)
    {
    case DEV_UNKNOWN:
        return "UNKNOWN";
    case DEV_REQUESTED:
        return "REQUESTED";
    case DEV_PROVISIONED:
        return "PROVISIONED";
    case DEV_REVOKED:
        return "REVOKED";
    default:
        return "?";
    }
}
static esp_err_t db_save()
{
    nvs_handle_t h;
    esp_err_t e = nvs_open("gwdb", NVS_READWRITE, &h);
    if (e)
        return e;
    e = nvs_set_blob(h, "devdb", g_db, sizeof(g_db));
    if (!e)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}

static esp_err_t db_load()
{
    size_t need = 0;
    nvs_handle_t h;
    if (nvs_open("gwdb", NVS_READONLY, &h) != ESP_OK)
    {
        memset(g_db, 0, sizeof(g_db));
        return ESP_OK;
    }
    esp_err_t e = nvs_get_blob(h, "devdb", NULL, &need);
    if (e == ESP_OK && need == sizeof(g_db))
        e = nvs_get_blob(h, "devdb", g_db, &need);
    nvs_close(h);
    if (e != ESP_OK)
        memset(g_db, 0, sizeof(g_db));
    return ESP_OK;
}
int provision_find_uid(const uint8_t uid[8])
{
    for (int i = 0; i < MAX_DEVICES; i++)
        if (g_db[i].in_use && memcmp(g_db[i].uid, uid, 8) == 0)
            return i;
    return -1;
}
int provision_alloc_uid(const uint8_t uid[8])
{
    for (int i = 0; i < MAX_DEVICES; i++)
        if (!g_db[i].in_use)
        {
            memset(&g_db[i], 0, sizeof(g_db[i]));
            memcpy(g_db[i].uid, uid, 8);
            g_db[i].state = DEV_REQUESTED;
            g_db[i].in_use = 1;
            return i;
        }
    return -1;
}
bool provision_db_save(void) { return db_save(); }
static void uid_inc(uint8_t c[8])
{
    for (int i = 7; i >= 0; i--)
    {
        if (++c[i] != 0)
            break;
    }
}

void provision_uid_generate(uint8_t out_uid[8])
{
    memcpy(out_uid, g_gw_id, 6);
    out_uid[6] = g_uid_counter[6];
    out_uid[7] = g_uid_counter[7];
    uid_inc(g_uid_counter);
}

void provision_init(void)
{
    if (db_load() != ESP_OK)
        return;
    esp_read_mac(g_gw_id, ESP_MAC_WIFI_STA);
    memset(g_uid_counter, 0, sizeof(g_uid_counter));

    printf("Gateway ID: %02X:%02X:%02X:%02X:%02X:%02X\n",
           g_gw_id[0], g_gw_id[1], g_gw_id[2], g_gw_id[3], g_gw_id[4], g_gw_id[5]);
    provision_uid_generate(g_uid_counter);
    printf("Starting UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
           g_uid_counter[0], g_uid_counter[1], g_uid_counter[2], g_uid_counter[3],
           g_uid_counter[4], g_uid_counter[5], g_uid_counter[6], g_uid_counter[7]);
    provision_uid_generate(g_uid_counter);
    printf("Next UID:     %02X%02X%02X%02X%02X%02X%02X%02X\n",
           g_uid_counter[0], g_uid_counter[1], g_uid_counter[2], g_uid_counter[3],
           g_uid_counter[4], g_uid_counter[5], g_uid_counter[6], g_uid_counter[7]);
}

void provision_fill_header(header_t *h, uint8_t msg_type, uint8_t flags, uint16_t seq16)
{
    h->msg_type = msg_type;
    h->flags = flags;
    h->seq16 = seq16;
}

offer_t provision_make_offer(const uint8_t proposed_uid[8], const uint8_t gw_id[6], uint16_t echo_nonce, uint16_t seq16)
{
    offer_t o;
    memset(&o, 0, sizeof(o));
    provision_fill_header(&o.h, MSG_OFFER, 0, seq16);
    memcpy(o.gw_id, gw_id ? gw_id : g_gw_id, 6);
    memcpy(o.proposed_uid, proposed_uid, 8);
    o.echo_nonce = echo_nonce;
    return o;
}

void provision_fill_offer(offer_t *o, const uint8_t proposed_uid[8], const uint8_t gw_id[6], uint16_t echo_nonce, uint16_t seq16)
{
    memset(o, 0, sizeof(*o));
    *o = provision_make_offer(proposed_uid, gw_id, echo_nonce, seq16);
}

ack_t provision_make_ack(const uint8_t assigned_uid[8], const uint8_t gw_id[6], uint8_t status, uint16_t seq16)
{
    ack_t a;
    memset(&a, 0, sizeof(a));
    provision_fill_header(&a.h, MSG_ACK, 0, seq16);
    memcpy(a.assigned_uid, assigned_uid, 8);
    memcpy(a.gw_id, gw_id ? gw_id : g_gw_id, 6);
    a.status = status;
    return a;
}

void provision_fill_ack(ack_t *a, const uint8_t assigned_uid[8], const uint8_t gw_id[6], uint8_t status, uint16_t seq16)
{
    memset(a, 0, sizeof(*a));
    *a = provision_make_ack(assigned_uid, gw_id, status, seq16);
}

static inline bool is_zero_uid(const uint8_t uid[8])
{
    for (int i = 0; i < 8; i++)
        if (uid[i])
            return false;
    return true;
}
bool provision_handle_hello(const void *buf, size_t len, int16_t rssi, int8_t snr, prov_result_t *out)
{
    if (len < sizeof(hello_t))
        return false;
    const hello_t *p = (const hello_t *)buf;

    memset(out, 0, sizeof(*out));

    if (is_zero_uid(p->current_uid))
    {
        // case mà chưa có UID, cần cấp mới
        uint8_t uid_new[8];
        provision_uid_generate(uid_new);
        out->act = PROV_ACT_SEND_OFFER;
        out->frame.offer = provision_make_offer(uid_new, NULL, p->nonce, p->h.seq16);
        return true;
    }

    int idx = provision_find_uid(p->current_uid);
    if (idx < 0)
    {
        idx = provision_alloc_uid(p->current_uid);
        if (idx < 0)
        {
            out->act = PROV_ACT_SEND_ACK;
            out->frame.ack = provision_make_ack(p->current_uid, NULL, /*DENY*/ 1, p->h.seq16);
            return true;
        }
        g_db[idx].state = DEV_PROVISIONED;
        g_db[idx].last_rssi = rssi;
        g_db[idx].last_snr = snr;
        g_db[idx].last_seen_ms = millis();
        provision_db_save();
        out->act = PROV_ACT_SEND_ACK;
        out->frame.ack = provision_make_ack(p->current_uid, NULL, /*OK*/ 0, p->h.seq16);
        return true;
    }
    if (g_db[idx].state == DEV_REVOKED)
    {
        out->act = PROV_ACT_SEND_ACK;
        out->frame.ack = provision_make_ack(p->current_uid, NULL, /*DENY*/ 1, p->h.seq16);
        return true;
    }
    g_db[idx].last_rssi = rssi;
    g_db[idx].last_snr = snr;
    g_db[idx].last_seen_ms = millis();
    provision_db_save();
    out->act = PROV_ACT_SEND_ACK;
    out->frame.ack = provision_make_ack(p->current_uid, NULL, /*OK*/ 0, p->h.seq16);
    return true;
}

bool provision_handle_request(const void *buf, size_t len, prov_result_t *out)
{
    if (len < sizeof(request_t))
        return false;
    const request_t *r = (const request_t *)buf;

    if (memcmp(r->gw_id, g_gw_id, 6) != 0)
    {
        out->act = PROV_ACT_NONE;
        return true;
    }

    int idx = provision_find_uid(r->proposed_uid);
    if (idx < 0)
        idx = provision_alloc_uid(r->proposed_uid);

    memset(out, 0, sizeof(*out));
    out->act = PROV_ACT_SEND_ACK;
    if (idx >= 0)
    {
        g_db[idx].state = DEV_PROVISIONED;
        provision_db_save();
        out->frame.ack = provision_make_ack(r->proposed_uid, NULL, /*OK*/ 0, r->h.seq16);
    }
    else
    {
        out->frame.ack = provision_make_ack(r->proposed_uid, NULL, /*DENY*/ 1, r->h.seq16);
    }
    return true;
}

bool provision_handle_data(const void *buf, size_t len, int16_t rssi, int8_t snr, bool *accepted)
{
    *accepted = false;
    if (len < sizeof(data_t))
        return false;
    const data_t *d = (const data_t *)buf;

    int idx = provision_find_uid(d->uid);
    if (idx < 0 || g_db[idx].state != DEV_PROVISIONED)
        return true;
    if (d->seq32 <= g_db[idx].last_seq)
        return true;

    g_db[idx].last_seq = d->seq32;
    g_db[idx].last_rssi = rssi;
    g_db[idx].last_snr = snr;
    g_db[idx].last_seen_ms = millis();
    provision_db_save();
    *accepted = true;
    return true;
}