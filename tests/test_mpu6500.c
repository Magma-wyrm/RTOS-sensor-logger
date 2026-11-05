#include "test.h"
#include "mpu6500.h"
#include "imu_model.h"

static mpu6500_t make_dev(void)
{
    mpu6500_t d = {
        .addr = MPU6500_ADDR,
        .write = imu_model_i2c_write,
        .read = imu_model_i2c_read,
        .who_am_i = 0,
    };
    return d;
}

static void mpu_init_configures_registers(void)
{
    mpu6500_t dev = make_dev();
    imu_model_reset();

    CHECK_EQ(mpu6500_init(&dev), MPU6500_OK);
    CHECK_EQ(dev.who_am_i, 0x70);
    CHECK_EQ(imu_model_get_reg(MPU6500_REG_PWR_MGMT_1), 0x01);   /* awake, PLL clock */
    CHECK_EQ(imu_model_get_reg(MPU6500_REG_SMPLRT_DIV), 9);      /* 100 Hz */
    CHECK_EQ(imu_model_get_reg(MPU6500_REG_CONFIG), 0x03);
    CHECK_EQ(imu_model_get_reg(MPU6500_REG_GYRO_CFG), 0x08);
    CHECK_EQ(imu_model_get_reg(MPU6500_REG_ACCEL_CFG), 0x08);
}

static void mpu_init_bus_error(void)
{
    mpu6500_t dev = make_dev();
    imu_model_reset();
    imu_model_fail_next(1);
    CHECK_EQ(mpu6500_init(&dev), MPU6500_ERR_BUS);
}

static void mpu_wrong_address_is_bus_error(void)
{
    mpu6500_t dev = make_dev();
    dev.addr = 0x69;   /* AD0 high, nobody there in the model */
    imu_model_reset();
    CHECK_EQ(mpu6500_init(&dev), MPU6500_ERR_BUS);
}

/* fake bus that answers with a wrong chip id */
static int read_wrong_id(uint8_t a, uint8_t reg, uint8_t *data, uint16_t len)
{
    (void)a; (void)reg;
    for (uint16_t i = 0; i < len; i++) data[i] = 0x12;
    return 0;
}

static void mpu_wrong_id_rejected(void)
{
    mpu6500_t dev = make_dev();
    dev.read = read_wrong_id;
    CHECK_EQ(mpu6500_init(&dev), MPU6500_ERR_ID);
    CHECK_EQ(dev.who_am_i, 0x12);
}

static void mpu_read_raw_values(void)
{
    mpu6500_t dev = make_dev();
    mpu6500_raw_t r;
    imu_model_reset();
    mpu6500_init(&dev);

    CHECK_EQ(mpu6500_read_raw(&dev, &r), MPU6500_OK);
    CHECK_EQ(r.az, 8192);   /* model gives +1 g on z */
    CHECK_EQ(r.gz, 10);
    CHECK_EQ(imu_model_read_count(), 1);
}

static void mpu_unpack_big_endian_negative(void)
{
    uint8_t raw[14] = { 0xFF, 0xFE, 0x80, 0x00, 0x7F, 0xFF, 0, 0, 0x00, 0x01, 0, 0, 0, 0 };
    mpu6500_raw_t r;
    mpu6500_unpack(raw, &r);
    CHECK_EQ(r.ax, -2);
    CHECK_EQ(r.ay, -32768);
    CHECK_EQ(r.az, 32767);
    CHECK_EQ(r.gx, 1);
}

static void mpu_read_error_reported(void)
{
    mpu6500_t dev = make_dev();
    mpu6500_raw_t r;
    imu_model_reset();
    mpu6500_init(&dev);
    imu_model_fail_next(1);
    CHECK_EQ(mpu6500_read_raw(&dev, &r), MPU6500_ERR_BUS);
    CHECK_EQ(mpu6500_read_raw(&dev, &r), MPU6500_OK);   /* recovers on next read */
}

void run_mpu6500_tests(void)
{
    RUN_TEST(mpu_init_configures_registers);
    RUN_TEST(mpu_init_bus_error);
    RUN_TEST(mpu_wrong_address_is_bus_error);
    RUN_TEST(mpu_wrong_id_rejected);
    RUN_TEST(mpu_read_raw_values);
    RUN_TEST(mpu_unpack_big_endian_negative);
    RUN_TEST(mpu_read_error_reported);
}
