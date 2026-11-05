/* I2C1 master, register level, following the sequences in RM0368
 * section 18.3.3 (master transmitter / receiver).
 *
 * NOTE: written from the reference manual, compiles, but NOT tested on
 * the real board yet (my hardware is not with me right now). The sim and
 * QEMU builds use the fake IMU instead of this file. */
#include "i2c1.h"
#include "stm32f401_regs.h"
#include "bsp.h"

#define I2C_TIMEOUT_US 2000u

static int wait_sr1(uint32_t mask)
{
    uint32_t start = bsp_time_us();
    while ((I2C1_SR1 & mask) == 0) {
        if (I2C1_SR1 & I2C_SR1_AF) {
            return -1;   /* NACK from the slave */
        }
        if (bsp_time_us() - start > I2C_TIMEOUT_US) {
            return -1;
        }
    }
    return 0;
}

static int fail(void)
{
    I2C1_CR1 |= I2C_CR1_STOP;
    I2C1_SR1 &= ~I2C_SR1_AF;   /* clear NACK flag */
    I2C1_CR1 &= ~I2C_CR1_POS;
    return -1;
}

static void clear_addr(void)
{
    volatile uint32_t tmp;
    tmp = I2C1_SR1;   /* ADDR is cleared by reading SR1 then SR2 */
    tmp = I2C1_SR2;
    (void)tmp;
}

void i2c1_init(uint32_t pclk1_hz)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB6, PB7: alternate function 4, open drain, pull-up, high speed */
    GPIO_MODER(GPIOB_BASE) = (GPIO_MODER(GPIOB_BASE) & ~(0xFu << 12)) | (0xAu << 12);
    GPIO_OTYPER(GPIOB_BASE) |= (1u << 6) | (1u << 7);
    GPIO_OSPEEDR(GPIOB_BASE) |= (0xFu << 12);
    GPIO_PUPDR(GPIOB_BASE) = (GPIO_PUPDR(GPIOB_BASE) & ~(0xFu << 12)) | (0x5u << 12);
    GPIO_AFRL(GPIOB_BASE) = (GPIO_AFRL(GPIOB_BASE) & ~(0xFFu << 24)) | (0x44u << 24);

    I2C1_CR1 = I2C_CR1_SWRST;   /* reset in case the bus was left in a bad state */
    I2C1_CR1 = 0;

    uint32_t mhz = pclk1_hz / 1000000u;   /* 42 */
    I2C1_CR2 = mhz;
    /* fast mode, duty 2:1 -> T_high = CCR*Tpclk, T_low = 2*CCR*Tpclk
     * CCR = 42 MHz / (3 * 400 kHz) = 35 */
    I2C1_CCR = I2C_CCR_FS | (pclk1_hz / (3u * 400000u));
    /* max rise time in fast mode is 300 ns -> 300 ns * 42 MHz + 1 = 13 */
    I2C1_TRISE = (mhz * 300u) / 1000u + 1u;
    I2C1_CR1 = I2C_CR1_PE;
}

/* START, address + write, register byte. Used by both read and write */
static int start_and_reg(uint8_t dev_addr, uint8_t reg)
{
    uint32_t start = bsp_time_us();
    while (I2C1_SR2 & I2C_SR2_BUSY) {
        if (bsp_time_us() - start > I2C_TIMEOUT_US) {
            return -1;
        }
    }
    I2C1_CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) != 0) return -1;
    I2C1_DR = (uint32_t)(dev_addr << 1);
    if (wait_sr1(I2C_SR1_ADDR) != 0) return -1;
    clear_addr();
    if (wait_sr1(I2C_SR1_TXE) != 0) return -1;
    I2C1_DR = reg;
    return 0;
}

int i2c1_write_reg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (start_and_reg(dev_addr, reg) != 0) {
        return fail();
    }
    for (uint16_t i = 0; i < len; i++) {
        if (wait_sr1(I2C_SR1_TXE) != 0) return fail();
        I2C1_DR = data[i];
    }
    if (wait_sr1(I2C_SR1_BTF) != 0) return fail();
    I2C1_CR1 |= I2C_CR1_STOP;
    return 0;
}

int i2c1_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (len == 0) {
        return 0;
    }
    if (start_and_reg(dev_addr, reg) != 0) {
        return fail();
    }
    if (wait_sr1(I2C_SR1_BTF) != 0) return fail();

    /* repeated start, address + read */
    I2C1_CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) != 0) return fail();
    I2C1_DR = (uint32_t)((dev_addr << 1) | 1u);

    if (len == 1) {
        if (wait_sr1(I2C_SR1_ADDR) != 0) return fail();
        I2C1_CR1 &= ~I2C_CR1_ACK;          /* NACK the only byte */
        clear_addr();
        I2C1_CR1 |= I2C_CR1_STOP;
        if (wait_sr1(I2C_SR1_RXNE) != 0) return fail();
        data[0] = (uint8_t)I2C1_DR;
        return 0;
    }

    if (len == 2) {
        if (wait_sr1(I2C_SR1_ADDR) != 0) return fail();
        I2C1_CR1 |= I2C_CR1_POS;
        I2C1_CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        if (wait_sr1(I2C_SR1_BTF) != 0) return fail();
        I2C1_CR1 |= I2C_CR1_STOP;
        data[0] = (uint8_t)I2C1_DR;
        data[1] = (uint8_t)I2C1_DR;
        I2C1_CR1 &= ~I2C_CR1_POS;
        return 0;
    }

    /* len > 2: ACK every byte until 3 are left, then the special ending */
    I2C1_CR1 |= I2C_CR1_ACK;
    if (wait_sr1(I2C_SR1_ADDR) != 0) return fail();
    clear_addr();

    uint16_t i = 0;
    while (len - i > 3) {
        if (wait_sr1(I2C_SR1_RXNE) != 0) return fail();
        data[i++] = (uint8_t)I2C1_DR;
    }
    /* 3 bytes left: wait until N-2 is in DR and N-1 in the shift register */
    if (wait_sr1(I2C_SR1_BTF) != 0) return fail();
    I2C1_CR1 &= ~I2C_CR1_ACK;
    data[i++] = (uint8_t)I2C1_DR;          /* N-2 */
    if (wait_sr1(I2C_SR1_BTF) != 0) return fail();
    I2C1_CR1 |= I2C_CR1_STOP;
    data[i++] = (uint8_t)I2C1_DR;          /* N-1 */
    if (wait_sr1(I2C_SR1_RXNE) != 0) return fail();
    data[i++] = (uint8_t)I2C1_DR;          /* N */
    return 0;
}
