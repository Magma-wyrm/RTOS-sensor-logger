#include "imu_model.h"
#include "mpu6500.h"

static uint8_t regs[128];
static uint32_t fail_count;
static uint32_t burst_reads;

/* small lookup table for a sine wave (one period = 32 samples), amplitude 1000 */
static const int16_t sine32[32] = {
       0,  195,  383,  556,  707,  831,  924,  981,
    1000,  981,  924,  831,  707,  556,  383,  195,
       0, -195, -383, -556, -707, -831, -924, -981,
   -1000, -981, -924, -831, -707, -556, -383, -195
};

void imu_model_reset(void)
{
    for (int i = 0; i < 128; i++) {
        regs[i] = 0;
    }
    regs[MPU6500_REG_WHO_AM_I] = MPU6500_WHOAMI_6500;
    regs[MPU6500_REG_PWR_MGMT_1] = 0x40;   /* reset value: SLEEP bit set */
    fail_count = 0;
    burst_reads = 0;
}

void imu_model_fail_next(uint32_t n)
{
    fail_count = n;
}

uint8_t imu_model_get_reg(uint8_t reg)
{
    return regs[reg & 0x7F];
}

uint32_t imu_model_read_count(void)
{
    return burst_reads;
}

static void put_be16(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)((uint16_t)v >> 8);
    p[1] = (uint8_t)((uint16_t)v & 0xFF);
}

/* fills the output registers with new "measurements" */
static void update_outputs(void)
{
    int16_t s = sine32[burst_reads % 32];

    put_be16(&regs[0x3B], s);            /* ax */
    put_be16(&regs[0x3D], (int16_t)(s / 2)); /* ay */
    put_be16(&regs[0x3F], 8192);         /* az = +1 g at +-4 g range */
    put_be16(&regs[0x41], 2500);         /* temp raw */
    put_be16(&regs[0x43], (int16_t)(-s));    /* gx */
    put_be16(&regs[0x45], 0);            /* gy */
    put_be16(&regs[0x47], 10);           /* gz small offset like a real gyro */
}

int imu_model_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (fail_count > 0) {
        fail_count--;
        return -1;
    }
    if (dev_addr != MPU6500_ADDR) {
        return -1;   /* nobody answers at this address -> NACK */
    }
    for (uint16_t i = 0; i < len; i++) {
        uint8_t r = (uint8_t)((reg + i) & 0x7F);
        if (r == MPU6500_REG_WHO_AM_I) {
            continue;   /* read only */
        }
        regs[r] = data[i];
    }
    return 0;
}

int imu_model_i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (fail_count > 0) {
        fail_count--;
        return -1;
    }
    if (dev_addr != MPU6500_ADDR) {
        return -1;
    }
    if (reg == MPU6500_REG_ACCEL_XOUT) {
        update_outputs();
        burst_reads++;
    }
    for (uint16_t i = 0; i < len; i++) {
        data[i] = regs[(reg + i) & 0x7F];
    }
    return 0;
}
