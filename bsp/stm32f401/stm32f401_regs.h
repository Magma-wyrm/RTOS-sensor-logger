#ifndef STM32F401_REGS_H
#define STM32F401_REGS_H

/* Only the registers this project uses, written by hand from the
 * STM32F401 reference manual (RM0368). I did not use the ST HAL here
 * on purpose, to learn what the peripherals actually do. */

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---------- RCC (RM0368 section 6) ---------- */
#define RCC_BASE        0x40023800u
#define RCC_CR          REG32(RCC_BASE + 0x00)
#define RCC_PLLCFGR     REG32(RCC_BASE + 0x04)
#define RCC_CFGR        REG32(RCC_BASE + 0x08)
#define RCC_AHB1ENR     REG32(RCC_BASE + 0x30)
#define RCC_APB1ENR     REG32(RCC_BASE + 0x40)
#define RCC_APB2ENR     REG32(RCC_BASE + 0x44)
#define RCC_CSR         REG32(RCC_BASE + 0x74)

#define RCC_CR_HSEON        (1u << 16)
#define RCC_CR_HSERDY       (1u << 17)
#define RCC_CR_PLLON        (1u << 24)
#define RCC_CR_PLLRDY       (1u << 25)
#define RCC_CFGR_SW_PLL     (2u << 0)
#define RCC_CFGR_SWS_MASK   (3u << 2)
#define RCC_CFGR_SWS_PLL    (2u << 2)
#define RCC_CFGR_PPRE1_DIV2 (4u << 10)
#define RCC_CSR_RMVF        (1u << 24)
#define RCC_CSR_IWDGRSTF    (1u << 29)

#define RCC_AHB1ENR_GPIOAEN (1u << 0)
#define RCC_AHB1ENR_GPIOBEN (1u << 1)
#define RCC_AHB1ENR_GPIOCEN (1u << 2)
#define RCC_APB1ENR_TIM2EN  (1u << 0)
#define RCC_APB1ENR_USART2EN (1u << 17)
#define RCC_APB1ENR_I2C1EN  (1u << 21)
#define RCC_APB2ENR_USART1EN (1u << 4)

/* ---------- FLASH ---------- */
#define FLASH_ACR       REG32(0x40023C00u)
#define FLASH_ACR_LATENCY_2WS (2u << 0)
#define FLASH_ACR_PRFTEN    (1u << 8)
#define FLASH_ACR_ICEN      (1u << 9)
#define FLASH_ACR_DCEN      (1u << 10)

/* ---------- GPIO ---------- */
#define GPIOA_BASE      0x40020000u
#define GPIOB_BASE      0x40020400u
#define GPIOC_BASE      0x40020800u
#define GPIO_MODER(b)   REG32((b) + 0x00)
#define GPIO_OTYPER(b)  REG32((b) + 0x04)
#define GPIO_OSPEEDR(b) REG32((b) + 0x08)
#define GPIO_PUPDR(b)   REG32((b) + 0x0C)
#define GPIO_IDR(b)     REG32((b) + 0x10)
#define GPIO_ODR(b)     REG32((b) + 0x14)
#define GPIO_BSRR(b)    REG32((b) + 0x18)
#define GPIO_AFRL(b)    REG32((b) + 0x20)
#define GPIO_AFRH(b)    REG32((b) + 0x24)

#define GPIO_MODE_INPUT  0u
#define GPIO_MODE_OUTPUT 1u
#define GPIO_MODE_AF     2u

/* ---------- USART (USART1 on APB2, USART2 on APB1) ---------- */
#define USART1_BASE     0x40011000u
#define USART2_BASE     0x40004400u
#define USART_SR(b)     REG32((b) + 0x00)
#define USART_DR(b)     REG32((b) + 0x04)
#define USART_BRR(b)    REG32((b) + 0x08)
#define USART_CR1(b)    REG32((b) + 0x0C)

#define USART_SR_ORE    (1u << 3)
#define USART_SR_RXNE   (1u << 5)
#define USART_SR_TC     (1u << 6)
#define USART_SR_TXE    (1u << 7)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR1_RXNEIE (1u << 5)
#define USART_CR1_UE    (1u << 13)

/* ---------- I2C1 ---------- */
#define I2C1_BASE       0x40005400u
#define I2C1_CR1        REG32(I2C1_BASE + 0x00)
#define I2C1_CR2        REG32(I2C1_BASE + 0x04)
#define I2C1_DR         REG32(I2C1_BASE + 0x10)
#define I2C1_SR1        REG32(I2C1_BASE + 0x14)
#define I2C1_SR2        REG32(I2C1_BASE + 0x18)
#define I2C1_CCR        REG32(I2C1_BASE + 0x1C)
#define I2C1_TRISE      REG32(I2C1_BASE + 0x20)

#define I2C_CR1_PE      (1u << 0)
#define I2C_CR1_START   (1u << 8)
#define I2C_CR1_STOP    (1u << 9)
#define I2C_CR1_ACK     (1u << 10)
#define I2C_CR1_POS     (1u << 11)
#define I2C_CR1_SWRST   (1u << 15)
#define I2C_SR1_SB      (1u << 0)
#define I2C_SR1_ADDR    (1u << 1)
#define I2C_SR1_BTF     (1u << 2)
#define I2C_SR1_RXNE    (1u << 6)
#define I2C_SR1_TXE     (1u << 7)
#define I2C_SR1_AF      (1u << 10)
#define I2C_SR2_BUSY    (1u << 1)
#define I2C_CCR_FS      (1u << 15)

/* ---------- TIM2 (32 bit timer, used as microsecond clock) ---------- */
#define TIM2_BASE       0x40000000u
#define TIM2_CR1        REG32(TIM2_BASE + 0x00)
#define TIM2_EGR        REG32(TIM2_BASE + 0x14)
#define TIM2_CNT        REG32(TIM2_BASE + 0x24)
#define TIM2_PSC        REG32(TIM2_BASE + 0x28)
#define TIM2_ARR        REG32(TIM2_BASE + 0x2C)

/* ---------- IWDG ---------- */
#define IWDG_BASE       0x40003000u
#define IWDG_KR         REG32(IWDG_BASE + 0x00)
#define IWDG_PR         REG32(IWDG_BASE + 0x04)
#define IWDG_RLR        REG32(IWDG_BASE + 0x08)
#define IWDG_SR         REG32(IWDG_BASE + 0x0C)

/* ---------- Cortex-M4 core ---------- */
#define SCB_CPACR       REG32(0xE000ED88u)
#define SCB_CFSR        REG32(0xE000ED28u)
#define NVIC_ISER(n)    REG32(0xE000E100u + 4u * (n))
#define NVIC_IPR_BYTE(irq) (*(volatile uint8_t *)(0xE000E400u + (irq)))

#define USART1_IRQn     37
#define USART2_IRQn     38

#endif
