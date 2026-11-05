#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>

/* MPU6500 register map (from the MPU-6500 register map document) */
#define MPU6500_ADDR            0x68   /* AD0 pin low */
#define MPU6500_REG_SMPLRT_DIV  0x19
#define MPU6500_REG_CONFIG      0x1A
#define MPU6500_REG_GYRO_CFG    0x1B
#define MPU6500_REG_ACCEL_CFG   0x1C
#define MPU6500_REG_ACCEL_XOUT  0x3B   /* 14 bytes: accel xyz, temp, gyro xyz (big endian) */
#define MPU6500_REG_PWR_MGMT_1  0x6B
#define MPU6500_REG_WHO_AM_I    0x75

#define MPU6500_WHOAMI_6500     0x70
#define MPU6500_WHOAMI_9250     0x71   /* MPU9250 has an MPU6500 inside, same registers */

/* The driver does not know about STM32 or Linux. The board code gives it
 * two functions that do the actual I2C transfer. Return 0 on success. */
typedef int (*mpu_i2c_write_fn)(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
typedef int (*mpu_i2c_read_fn)(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

typedef struct {
    uint8_t addr;
    mpu_i2c_write_fn write;
    mpu_i2c_read_fn read;
    uint8_t who_am_i;     /* filled by init */
} mpu6500_t;

typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} mpu6500_raw_t;

#define MPU6500_OK          0
#define MPU6500_ERR_BUS    -1
#define MPU6500_ERR_ID     -2

int mpu6500_init(mpu6500_t *dev);
int mpu6500_read_raw(mpu6500_t *dev, mpu6500_raw_t *out);

/* converts the 14 raw bytes (big endian) into the struct, kept separate so it can be unit tested */
void mpu6500_unpack(const uint8_t raw[14], mpu6500_raw_t *out);

#endif
