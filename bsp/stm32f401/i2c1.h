#ifndef I2C1_H
#define I2C1_H

#include <stdint.h>

/* Polling I2C1 master driver, 400 kHz, PB6 = SCL, PB7 = SDA.
 * Every wait has a timeout so a missing sensor cannot hang the task. */
void i2c1_init(uint32_t pclk1_hz);
int i2c1_write_reg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
int i2c1_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

#endif
