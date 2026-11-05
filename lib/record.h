#ifndef RECORD_H
#define RECORD_H

#include <stdint.h>

/* One IMU sample as it travels through the queue (not packed, just a C struct) */
typedef struct {
    uint32_t seq;      /* increments every sample period, even if the sample is dropped */
    uint32_t t_us;     /* timestamp taken right after the sampling task wakes up */
    int16_t  ax, ay, az;
    int16_t  gx, gy, gz;
    uint8_t  flags;
} sample_t;

#define SAMPLE_FLAG_IMU_ERROR   0x01   /* I2C read failed, values are zero */
#define SAMPLE_FLAG_LATE        0x02   /* xTaskDelayUntil said we woke up late */

/* On-disk / on-wire record format (little endian), 24 bytes:
 *  [0]      sync byte 0xA5
 *  [1]      flags
 *  [2..5]   seq
 *  [6..9]   t_us
 *  [10..21] ax ay az gx gy gz (int16 each)
 *  [22..23] CRC16-CCITT of bytes 0..21
 * I write the bytes one by one instead of memcpy of a struct so the
 * format does not depend on compiler padding or CPU endianness. */
#define RECORD_SYNC   0xA5
#define RECORD_SIZE   24

void record_encode(const sample_t *s, uint8_t out[RECORD_SIZE]);

/* returns 0 if ok, -1 bad sync byte, -2 bad CRC */
int record_decode(const uint8_t in[RECORD_SIZE], sample_t *s);

#endif
