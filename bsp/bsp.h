#ifndef BSP_H
#define BSP_H

#include <stdint.h>

/* Board support interface. The application (app/) only calls these
 * functions, so the same task code runs on:
 *   bsp/sim       - Linux, FreeRTOS POSIX port (for testing without hardware)
 *   bsp/stm32f401 - STM32F401CCU6 "Black Pill", FreeRTOS ARM_CM4F port
 *                   (also runs in QEMU netduinoplus2 when built with QEMU_BUILD) */

void bsp_init(void);                 /* clocks, pins, uarts, before the scheduler starts */
const char *bsp_name(void);

uint32_t bsp_time_us(void);          /* free running microsecond counter (wraps after ~71 min) */

/* text console. NOT allowed from the sampling task or from interrupts */
void bsp_log(const char *fmt, ...);

/* console input: returns 0 and a byte if one was received, -1 if nothing */
int bsp_console_getc(uint8_t *c);

/* I2C bus for the IMU, 0 = ok */
int bsp_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
int bsp_i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

/* storage / data sink for the binary log records. Only the logger task calls it.
 * On the sim it is a file, on the board it is a second UART to the PC. */
int bsp_sink_write(const uint8_t *data, uint32_t len);
void bsp_sink_inject_stall(uint32_t ms);   /* fault injection: next write takes this long */

/* hardware watchdog (IWDG on STM32, a host thread on the sim) */
void bsp_watchdog_start(uint32_t timeout_ms);
void bsp_watchdog_kick(void);
int bsp_last_reset_was_watchdog(void);

void bsp_led_toggle(void);

/* sim only: returns 1 when the run time given on the command line is over */
int bsp_run_finished(void);
void bsp_shutdown(int exit_code);

#endif
