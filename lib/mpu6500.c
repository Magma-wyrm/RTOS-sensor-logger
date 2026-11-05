#include "mpu6500.h"

static int write_reg(mpu6500_t *dev, uint8_t reg, uint8_t value)
{
    return dev->write(dev->addr, reg, &value, 1);
}

int mpu6500_init(mpu6500_t *dev)
{
    uint8_t id = 0;

    if (dev->read(dev->addr, MPU6500_REG_WHO_AM_I, &id, 1) != 0) {
        return MPU6500_ERR_BUS;
    }
    dev->who_am_i = id;
    if (id != MPU6500_WHOAMI_6500 && id != MPU6500_WHOAMI_9250) {
        return MPU6500_ERR_ID;
    }

    /* wake up, use the gyro PLL as clock source (better than internal 20 MHz osc) */
    if (write_reg(dev, MPU6500_REG_PWR_MGMT_1, 0x01) != 0) {
        return MPU6500_ERR_BUS;
    }
    /* DLPF config 3 -> ~41 Hz bandwidth, ok for 100 Hz sampling */
    if (write_reg(dev, MPU6500_REG_CONFIG, 0x03) != 0) {
        return MPU6500_ERR_BUS;
    }
    /* internal sample rate = 1 kHz / (1 + 9) = 100 Hz */
    if (write_reg(dev, MPU6500_REG_SMPLRT_DIV, 9) != 0) {
        return MPU6500_ERR_BUS;
    }
    /* gyro +-500 dps (FS_SEL = 1), accel +-4 g (AFS_SEL = 1) */
    if (write_reg(dev, MPU6500_REG_GYRO_CFG, 0x08) != 0) {
        return MPU6500_ERR_BUS;
    }
    if (write_reg(dev, MPU6500_REG_ACCEL_CFG, 0x08) != 0) {
        return MPU6500_ERR_BUS;
    }
    return MPU6500_OK;
}

void mpu6500_unpack(const uint8_t raw[14], mpu6500_raw_t *out)
{
    out->ax   = (int16_t)((raw[0] << 8) | raw[1]);
    out->ay   = (int16_t)((raw[2] << 8) | raw[3]);
    out->az   = (int16_t)((raw[4] << 8) | raw[5]);
    out->temp = (int16_t)((raw[6] << 8) | raw[7]);
    out->gx   = (int16_t)((raw[8] << 8) | raw[9]);
    out->gy   = (int16_t)((raw[10] << 8) | raw[11]);
    out->gz   = (int16_t)((raw[12] << 8) | raw[13]);
}

int mpu6500_read_raw(mpu6500_t *dev, mpu6500_raw_t *out)
{
    uint8_t raw[14];

    /* one burst read so accel and gyro values belong to the same sample */
    if (dev->read(dev->addr, MPU6500_REG_ACCEL_XOUT, raw, sizeof(raw)) != 0) {
        return MPU6500_ERR_BUS;
    }
    mpu6500_unpack(raw, out);
    return MPU6500_OK;
}
