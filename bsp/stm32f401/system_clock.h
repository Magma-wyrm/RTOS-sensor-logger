#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include <stdint.h>

extern uint32_t SystemCoreClock;   /* used by FreeRTOS for SysTick */
extern uint32_t g_pclk1_hz;        /* APB1: USART2, I2C1 */
extern uint32_t g_pclk2_hz;        /* APB2: USART1 */
extern uint32_t g_tim2_clk_hz;     /* TIM2 input clock */

/* returns 0 if running from HSE+PLL at 84 MHz, -1 if it fell back to HSI 16 MHz */
int system_clock_init(void);

#endif
