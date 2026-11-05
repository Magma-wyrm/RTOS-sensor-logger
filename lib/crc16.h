#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xor out.
 * Check value for "123456789" is 0x29B1 (used in the unit test). */
uint16_t crc16_ccitt(const uint8_t *data, uint32_t len);

#endif
