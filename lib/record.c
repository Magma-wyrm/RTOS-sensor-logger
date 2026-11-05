#include "record.h"
#include "crc16.h"

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void record_encode(const sample_t *s, uint8_t out[RECORD_SIZE])
{
    out[0] = RECORD_SYNC;
    out[1] = s->flags;
    put_u32(&out[2], s->seq);
    put_u32(&out[6], s->t_us);
    put_u16(&out[10], (uint16_t)s->ax);
    put_u16(&out[12], (uint16_t)s->ay);
    put_u16(&out[14], (uint16_t)s->az);
    put_u16(&out[16], (uint16_t)s->gx);
    put_u16(&out[18], (uint16_t)s->gy);
    put_u16(&out[20], (uint16_t)s->gz);

    uint16_t crc = crc16_ccitt(out, RECORD_SIZE - 2);
    put_u16(&out[22], crc);
}

int record_decode(const uint8_t in[RECORD_SIZE], sample_t *s)
{
    if (in[0] != RECORD_SYNC) {
        return -1;
    }
    if (crc16_ccitt(in, RECORD_SIZE - 2) != get_u16(&in[22])) {
        return -2;
    }

    s->flags = in[1];
    s->seq = get_u32(&in[2]);
    s->t_us = get_u32(&in[6]);
    s->ax = (int16_t)get_u16(&in[10]);
    s->ay = (int16_t)get_u16(&in[12]);
    s->az = (int16_t)get_u16(&in[14]);
    s->gx = (int16_t)get_u16(&in[16]);
    s->gy = (int16_t)get_u16(&in[18]);
    s->gz = (int16_t)get_u16(&in[20]);
    return 0;
}
