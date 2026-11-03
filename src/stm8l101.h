/* Minimal STM8L101F3 register definitions (RM0013). Only what this project uses. */
#ifndef STM8L101_H
#define STM8L101_H

#include <stdint.h>

#define SFR(addr)       (*(volatile uint8_t *)(addr))

/* GPIO */
#define PA_ODR          SFR(0x5000)
#define PA_IDR          SFR(0x5001)
#define PA_DDR          SFR(0x5002)
#define PA_CR1          SFR(0x5003)
#define PA_CR2          SFR(0x5004)

#define PB_ODR          SFR(0x5005)
#define PB_IDR          SFR(0x5006)
#define PB_DDR          SFR(0x5007)
#define PB_CR1          SFR(0x5008)
#define PB_CR2          SFR(0x5009)

#define PC_ODR          SFR(0x500A)
#define PC_DDR          SFR(0x500C)
#define PC_CR1          SFR(0x500D)

#define PD_ODR          SFR(0x500F)
#define PD_DDR          SFR(0x5011)
#define PD_CR1          SFR(0x5012)

/* Clock */
#define CLK_CKDIVR      SFR(0x50C0)   /* HSI divider: 0 = 16 MHz, reset value 3 = 2 MHz */
#define CLK_PCKENR      SFR(0x50C3)
#define CLK_PCKEN_TIM2  0x01
#define CLK_PCKEN_TIM3  0x02
#define CLK_PCKEN_TIM4  0x04

/* External interrupts */
#define EXTI_CR1        SFR(0x50A0)   /* P0IS..P3IS, 2 bits each */
#define EXTI_SR1        SFR(0x50A3)   /* P0F..P7F, write 1 to clear */
#define EXTI_SENS_FALL  0x02          /* falling edge only */

/* TIM2 / TIM3: 16-bit general purpose */
#define TIM2_CR1        SFR(0x5250)
#define TIM2_IER        SFR(0x5254)
#define TIM2_SR1        SFR(0x5255)
#define TIM2_EGR        SFR(0x5257)
#define TIM2_PSCR       SFR(0x525D)
#define TIM2_ARRH       SFR(0x525E)
#define TIM2_ARRL       SFR(0x525F)

#define TIM3_CR1        SFR(0x5280)
#define TIM3_IER        SFR(0x5284)
#define TIM3_SR1        SFR(0x5285)
#define TIM3_EGR        SFR(0x5287)
#define TIM3_PSCR       SFR(0x528D)
#define TIM3_ARRH       SFR(0x528E)
#define TIM3_ARRL       SFR(0x528F)

/* TIM4: 8-bit basic */
#define TIM4_CR1        SFR(0x52E0)
#define TIM4_IER        SFR(0x52E3)
#define TIM4_SR1        SFR(0x52E4)
#define TIM4_EGR        SFR(0x52E5)
#define TIM4_PSCR       SFR(0x52E7)
#define TIM4_ARR        SFR(0x52E8)

#define TIM_CR1_CEN     0x01
#define TIM_CR1_URS     0x04
#define TIM_CR1_ARPE    0x80
#define TIM_IER_UIE     0x01
#define TIM_SR1_UIF     0x01
#define TIM_EGR_UG      0x01

/* Interrupt vector numbers (datasheet IRQ numbering, as SDCC expects) */
#define IRQ_EXTI2       10
#define IRQ_TIM2_UPD    19
#define IRQ_TIM3_UPD    21
#define IRQ_TIM4_UPD    25

#define enable_irq()    __asm__("rim")
#define disable_irq()   __asm__("sim")
#define wfi()           __asm__("wfi")
#define halt()          __asm__("halt")
#define nop()           __asm__("nop")

#endif
