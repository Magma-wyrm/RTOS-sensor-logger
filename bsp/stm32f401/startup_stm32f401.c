/* Startup code and vector table for STM32F401 written in C
 * (instead of the usual startup_stm32f401xc.s from ST). */
#include <stdint.h>
#include "stm32f401_regs.h"

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

int main(void);
void bsp_panic_print(const char *s);

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);

/* FreeRTOS handlers (names mapped in FreeRTOSConfig.h) */
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

void USART1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

#define NUM_IRQS 85   /* STM32F401 has IRQ 0..84 */

/* -Woverride-init: the range fills everything with Default_Handler first,
 * then the lines after it replace the ones we use */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
__attribute__((section(".isr_vector"), used))
void (*const vector_table[16 + NUM_IRQS])(void) = {
    [0 ... (16 + NUM_IRQS - 1)] = Default_Handler,
    [0]  = (void (*)(void))(&_estack),
    [1]  = Reset_Handler,
    [3]  = HardFault_Handler,
    [7]  = 0, [8] = 0, [9] = 0, [10] = 0, [13] = 0,   /* reserved */
    [11] = SVC_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 + USART1_IRQn] = USART1_IRQHandler,
    [16 + USART2_IRQn] = USART2_IRQHandler,
};
#pragma GCC diagnostic pop

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    /* copy initialised variables from flash to RAM */
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    /* zero the .bss section */
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }
    /* enable the FPU (CP10 and CP11 full access), FreeRTOS CM4F port needs it */
    SCB_CPACR |= (0xFu << 20);
    __asm volatile("dsb\n isb");

    main();
    for (;;) {
    }
}

void Default_Handler(void)
{
    bsp_panic_print("FATAL: unexpected interrupt\r\n");
    for (;;) {
    }
}

/* saved for the debugger: look at these variables after a crash */
volatile uint32_t g_fault_pc;
volatile uint32_t g_fault_cfsr;

/* Find out which stack was in use and read the PC that was pushed on it.
 * With a debugger (ST-Link) you can then look up g_fault_pc in the .map file. */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b hardfault_c   \n");
}

__attribute__((used)) void hardfault_c(uint32_t *stack)
{
    g_fault_pc = stack[6];
    g_fault_cfsr = SCB_CFSR;
    bsp_panic_print("FATAL: hard fault (see g_fault_pc / g_fault_cfsr)\r\n");
    for (;;) {
        /* watchdog resets the board */
    }
}
