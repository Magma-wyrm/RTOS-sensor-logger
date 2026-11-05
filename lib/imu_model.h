#ifndef IMU_MODEL_H
#define IMU_MODEL_H

#include <stdint.h>

/* A fake MPU6500 that behaves like the real chip on the I2C level
 * (WHO_AM_I, config registers, 14 byte burst read from 0x3B).
 * Used in three places:
 *   - unit tests of the mpu6500 driver
 *   - the Linux simulation build
 *   - the QEMU build (QEMU does not emulate the I2C peripheral)
 * On the real board the same driver talks to the real chip instead. */

void imu_model_reset(void);
int imu_model_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
int imu_model_i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

/* fault injection: the next n transfers return an error (like a NACK) */
void imu_model_fail_next(uint32_t n);

uint8_t imu_model_get_reg(uint8_t reg);   /* for tests */
uint32_t imu_model_read_count(void);

#endif
