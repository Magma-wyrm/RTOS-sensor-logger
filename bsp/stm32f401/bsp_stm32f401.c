/* Board support for the STM32F401CCU6 Black Pill (and QEMU netduinoplus2).
 *
 * Pins:
 *   PA2 / PA3  USART2 TX / RX  console 115200 (USB-serial adapter)
 *   PA9        USART1 TX       binary log records 460800 -> PC
 *   PB6 / PB7  I2C1 SCL / SDA  MPU6500
 *   PC13       on-board LED (active low)
 */
#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bsp.h"
#include "stm32f401_regs.h"
#include "system_clock.h"
#include "i2c1.h"
#include "ringbuf.h"
#include "imu_model.h"

#define CONSOLE_BAUD 115200u
#define SINK_BAUD    460800u

static ringbuf_t console_rx;
static SemaphoreHandle_t console_mutex;
static char log_buf[256];
static int clock_ok;
static volatile uint32_t pending_stall_ms;
static int reset_by_watchdog;

/* ---------------- small helpers ---------------- */

static void gpio_set_af(uint32_t base, int pin, uint32_t af)
{
    GPIO_MODER(base) = (GPIO_MODER(base) & ~(3u << (pin * 2))) | (GPIO_MODE_AF << (pin * 2));
    if (pin < 8) {
        GPIO_AFRL(base) = (GPIO_AFRL(base) & ~(0xFu << (pin * 4))) | (af << (pin * 4));
    } else {
        GPIO_AFRH(base) = (GPIO_AFRH(base) & ~(0xFu << ((pin - 8) * 4))) | (af << ((pin - 8) * 4));
    }
}

static void uart_putc(uint32_t base, char c)
{
    while ((USART_SR(base) & USART_SR_TXE) == 0) {
    }
    USART_DR(base) = (uint8_t)c;
}

static void uart_puts(uint32_t base, const char *s)
{
    while (*s) {
        uart_putc(base, *s++);
    }
}

static uint32_t get_ipsr(void)
{
    uint32_t r;
    __asm volatile("mrs %0, ipsr" : "=r"(r));
    return r;
}

/* ---------------- init ---------------- */

static void uart_init(uint32_t base, uint32_t pclk, uint32_t baud, int rx)
{
    /* oversampling by 16: BRR = pclk / baud (mantissa and fraction together) */
    USART_BRR(base) = (pclk + baud / 2u) / baud;
    USART_CR1(base) = USART_CR1_UE | USART_CR1_TE | (rx ? (USART_CR1_RE | USART_CR1_RXNEIE) : 0u);
}

static void tim2_init(void)
{
    RCC_APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2_PSC = g_tim2_clk_hz / 1000000u - 1u;   /* 1 MHz -> 1 count = 1 us */
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_EGR = 1u;    /* load the prescaler now */
    TIM2_CR1 = 1u;    /* CEN */
}

void bsp_init(void)
{
    clock_ok = (system_clock_init() == 0);

    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    tim2_init();

    /* LED PC13 output, start off (high) */
    GPIO_BSRR(GPIOC_BASE) = (1u << 13);
    GPIO_MODER(GPIOC_BASE) = (GPIO_MODER(GPIOC_BASE) & ~(3u << 26)) | (GPIO_MODE_OUTPUT << 26);

    gpio_set_af(GPIOA_BASE, 2, 7);   /* USART2 TX */
    gpio_set_af(GPIOA_BASE, 3, 7);   /* USART2 RX */
    gpio_set_af(GPIOA_BASE, 9, 7);   /* USART1 TX */
    uart_init(USART2_BASE, g_pclk1_hz, CONSOLE_BAUD, 1);
    uart_init(USART1_BASE, g_pclk2_hz, SINK_BAUD, 0);

    ringbuf_init(&console_rx);
    /* RX interrupt priority must be numerically >= configMAX_SYSCALL_INTERRUPT_PRIORITY
     * if the ISR ever calls FreeRTOS. It does not, but keep it safe anyway. */
    NVIC_IPR_BYTE(USART2_IRQn) = (uint8_t)(6u << 4);
    NVIC_ISER(USART2_IRQn / 32) = 1u << (USART2_IRQn % 32);

#ifdef QEMU_BUILD
    imu_model_reset();
#else
    i2c1_init(g_pclk1_hz);
#endif

    reset_by_watchdog = (RCC_CSR & RCC_CSR_IWDGRSTF) != 0;
    RCC_CSR |= RCC_CSR_RMVF;   /* clear reset flags for next time */

    console_mutex = xSemaphoreCreateMutex();
}

const char *bsp_name(void)
{
#ifdef QEMU_BUILD
    return "STM32F4 in QEMU netduinoplus2 (fake IMU)";
#else
    return clock_ok ? "STM32F401CCU6 @ 84 MHz (HSE+PLL)" : "STM32F401CCU6 @ 16 MHz (HSI fallback!)";
#endif
}

/* ---------------- time ---------------- */

uint32_t bsp_time_us(void)
{
    return TIM2_CNT;
}

/* ---------------- console ---------------- */

void USART2_IRQHandler(void)
{
    uint32_t sr = USART_SR(USART2_BASE);
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t c = (uint8_t)USART_DR(USART2_BASE);   /* reading DR also clears ORE */
        ringbuf_put(&console_rx, c);
    }
}

/* used from fault handlers: no mutex, no RTOS */
void bsp_panic_print(const char *s)
{
    uart_puts(USART2_BASE, s);
}

void bsp_log(const char *fmt, ...)
{
    va_list ap;
    int use_mutex = (get_ipsr() == 0) &&
                    (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);

    /* The UART is shared by several tasks, so a mutex (with priority
     * inheritance) protects log_buf and the UART. Before the scheduler
     * runs, or from an exception handler, print directly. */
    if (use_mutex) {
        xSemaphoreTake(console_mutex, portMAX_DELAY);
    }
    va_start(ap, fmt);
    vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
    va_end(ap);
    uart_puts(USART2_BASE, log_buf);
    if (use_mutex) {
        xSemaphoreGive(console_mutex);
    }
}

int bsp_console_getc(uint8_t *c)
{
    return ringbuf_get(&console_rx, c);
}

/* ---------------- I2C ---------------- */

int bsp_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
#ifdef QEMU_BUILD
    return imu_model_i2c_write(dev_addr, reg, data, len);
#else
    return i2c1_write_reg(dev_addr, reg, data, len);
#endif
}

int bsp_i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
#ifdef QEMU_BUILD
    return imu_model_i2c_read(dev_addr, reg, data, len);
#else
    return i2c1_read_reg(dev_addr, reg, data, len);
#endif
}

/* ---------------- sink (USART1 to PC) ---------------- */

int bsp_sink_write(const uint8_t *data, uint32_t len)
{
    uint32_t stall = pending_stall_ms;
    if (stall > 0) {
        pending_stall_ms = 0;
        vTaskDelay(pdMS_TO_TICKS(stall));
    }
    /* polled: 768 bytes at 460800 baud is about 17 ms. The logger has
     * lower priority than sampling, so sampling still runs on time. */
    for (uint32_t i = 0; i < len; i++) {
        uart_putc(USART1_BASE, (char)data[i]);
    }
    return 0;
}

void bsp_sink_inject_stall(uint32_t ms)
{
    pending_stall_ms = ms;
}

/* ---------------- watchdog ---------------- */

void bsp_watchdog_start(uint32_t timeout_ms)
{
    /* LSI is about 32 kHz. Prescaler /64 (PR = 4) -> 500 Hz -> 2 ms per count */
    IWDG_KR = 0xCCCC;   /* start */
    IWDG_KR = 0x5555;   /* allow writes to PR and RLR */
    IWDG_PR = 4;
    IWDG_RLR = timeout_ms / 2u;
    for (uint32_t i = 0; i < 100000u && IWDG_SR != 0; i++) {
        /* wait until the new values are taken (SR = 0) */
    }
    IWDG_KR = 0xAAAA;   /* reload */
}

void bsp_watchdog_kick(void)
{
    IWDG_KR = 0xAAAA;
}

int bsp_last_reset_was_watchdog(void)
{
    return reset_by_watchdog;
}

void bsp_led_toggle(void)
{
    GPIO_ODR(GPIOC_BASE) ^= (1u << 13);
}

/* ---------------- run control ---------------- */

#ifdef QEMU_BUILD
#define QEMU_RUN_SECONDS 60
#endif

int bsp_run_finished(void)
{
#ifdef QEMU_BUILD
    return xTaskGetTickCount() >= (TickType_t)(QEMU_RUN_SECONDS * configTICK_RATE_HZ);
#else
    return 0;   /* the real board logs forever */
#endif
}

void bsp_shutdown(int exit_code)
{
#ifdef QEMU_BUILD
    /* ARM semihosting SYS_EXIT, makes QEMU quit (needs -semihosting) */
    register uint32_t r0 __asm("r0") = 0x18;
    register uint32_t r1 __asm("r1") = (exit_code == 0) ? 0x20026u : 0x20024u;
    __asm volatile("bkpt 0xAB" : : "r"(r0), "r"(r1) : "memory");
#else
    (void)exit_code;
#endif
    for (;;) {
    }
}

void vAssertCalled(const char *file, int line)
{
    (void)line;
    taskDISABLE_INTERRUPTS();
    bsp_panic_print("ASSERT failed in ");
    bsp_panic_print(file);
    bsp_panic_print("\r\n");
    for (;;) {
    }
}
