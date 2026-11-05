/* Clock setup for the Black Pill: 25 MHz crystal (HSE) -> PLL -> 84 MHz.
 *
 *   VCO in  = 25 MHz / M(25)  = 1 MHz
 *   VCO out = 1 MHz * N(336)  = 336 MHz
 *   SYSCLK  = 336 / P(4)      = 84 MHz
 *   USB     = 336 / Q(7)      = 48 MHz (not used yet)
 *   APB1    = 84 / 2 = 42 MHz (max for APB1), TIM2 gets 2x = 84 MHz
 *   APB2    = 84 MHz
 * Flash needs 2 wait states at 84 MHz and 3.3 V (RM0368 table 6).
 *
 * If the crystal does not start the code stays on the internal 16 MHz
 * oscillator instead of hanging forever. */
#include "system_clock.h"
#include "stm32f401_regs.h"

uint32_t SystemCoreClock = 16000000u;
uint32_t g_pclk1_hz = 16000000u;
uint32_t g_pclk2_hz = 16000000u;
uint32_t g_tim2_clk_hz = 16000000u;

#ifndef QEMU_BUILD
#define READY_TIMEOUT_LOOPS 200000u

static int wait_bit(volatile uint32_t *reg, uint32_t mask)
{
    for (uint32_t i = 0; i < READY_TIMEOUT_LOOPS; i++) {
        if ((*reg & mask) != 0) {
            return 0;
        }
    }
    return -1;
}
#endif

int system_clock_init(void)
{
#ifdef QEMU_BUILD
    /* QEMU netduinoplus2 has no RCC model and runs the core at a fixed
     * 168 MHz, its timers count at 1 GHz. Just use those numbers. */
    SystemCoreClock = 168000000u;
    g_pclk1_hz = 42000000u;
    g_pclk2_hz = 84000000u;
    g_tim2_clk_hz = 1000000000u;
    return 0;
#else
    RCC_CR |= RCC_CR_HSEON;
    if (wait_bit(&RCC_CR, RCC_CR_HSERDY) != 0) {
        return -1;   /* no crystal, stay on HSI 16 MHz */
    }

    FLASH_ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* AHB /1, APB1 /2, APB2 /1 */
    RCC_CFGR = (RCC_CFGR & ~((0xFu << 4) | (7u << 10) | (7u << 13))) | RCC_CFGR_PPRE1_DIV2;

    RCC_PLLCFGR = (25u << 0)      /* PLLM */
                | (336u << 6)     /* PLLN */
                | (1u << 16)      /* PLLP = 4 (bits 01) */
                | (1u << 22)      /* PLLSRC = HSE */
                | (7u << 24);     /* PLLQ */

    RCC_CR |= RCC_CR_PLLON;
    if (wait_bit(&RCC_CR, RCC_CR_PLLRDY) != 0) {
        return -1;
    }

    RCC_CFGR = (RCC_CFGR & ~3u) | RCC_CFGR_SW_PLL;
    for (uint32_t i = 0; i < READY_TIMEOUT_LOOPS; i++) {
        if ((RCC_CFGR & RCC_CFGR_SWS_MASK) == RCC_CFGR_SWS_PLL) {
            SystemCoreClock = 84000000u;
            g_pclk1_hz = 42000000u;
            g_pclk2_hz = 84000000u;
            g_tim2_clk_hz = 84000000u;   /* APB1 prescaler != 1 so timer clock is 2x */
            return 0;
        }
    }
    return -1;
#endif
}
