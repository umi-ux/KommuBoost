/* reg_defs.h - MINIMAL hand-written register definitions, STM32G030F6P6TR
 *
 * WHY THIS FILE EXISTS: the project has no CMSIS/HAL device header
 * available (no Drivers/ folder, CubeMX not usable). Rather than pull
 * in ST's full CMSIS package (a large third-party file you'd have to
 * trust wholesale for an ASIL B project), this defines ONLY the
 * registers this firmware actually touches: GPIOA (for FORCE_PT/
 * GATE_ENABLE) and the RCC clock-enable register.
 *
 * *** MANDATORY BEFORE FLASHING ***
 * Every address/offset below was written from memory of the STM32G0
 * reference manual (RM0444) structure, NOT copy-pasted from ST's own
 * header. Cross-check EVERY value marked "VERIFY" against your actual
 * RM0444 PDF before trusting this on real hardware:
 *   - Section 2.2  "Memory map"                  -> peripheral base addresses
 *   - GPIO chapter, "GPIO register map" table     -> GPIOx register offsets
 *   - RCC chapter,  "RCC register map" table      -> RCC register offsets
 * A wrong offset here means writing to the wrong register entirely -
 * this is exactly the kind of error that must be caught on paper
 * before it's caught on a bench with a real torque sensor attached.
 *
 * If/when you get access to ST's real CMSIS package later, prefer
 * switching to it - this file is a deliberate stand-in, not a
 * long-term replacement.
 */
#ifndef REG_DEFS_H
#define REG_DEFS_H

#include <stdint.h>

/* ---------------------------------------------------------------------
 * GPIO register block
 * VERIFY: offsets against RM0444 "GPIO register map" table.
 * --------------------------------------------------------------------- */
typedef struct {
    volatile uint32_t MODER;    /* 0x00 - mode register (2 bits/pin)          */
    volatile uint32_t OTYPER;   /* 0x04 - output type (1 bit/pin)             */
    volatile uint32_t OSPEEDR;  /* 0x08 - output speed (2 bits/pin)           */
    volatile uint32_t PUPDR;    /* 0x0C - pull-up/pull-down (2 bits/pin)      */
    volatile uint32_t IDR;      /* 0x10 - input data register (read pins)    */
    volatile uint32_t ODR;      /* 0x14 - output data register               */
    volatile uint32_t BSRR;     /* 0x18 - bit set/reset register             */
    volatile uint32_t LCKR;     /* 0x1C - config lock register               */
    volatile uint32_t AFR[2];   /* 0x20/0x24 - alternate function low/high   */
    volatile uint32_t BRR;      /* 0x28 - bit reset register (G0-specific)   */
} GPIO_TypeDef;

/* VERIFY: base addresses against RM0444 Section 2.2 memory map. */
#define GPIOA_BASE   (0x50000000UL)
#define GPIOB_BASE   (0x50000400UL)
#define GPIOA        ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB        ((GPIO_TypeDef *)GPIOB_BASE)

/* ---------------------------------------------------------------------
 * RCC register block - ONLY defined up through IOPENR (clock enable).
 * VERIFY: offsets against RM0444 "RCC register map" table -
 * particularly IOPENR's offset, since that's the one this file writes.
 * --------------------------------------------------------------------- */
typedef struct {
    volatile uint32_t CR;          /* 0x00 */
    volatile uint32_t ICSCR;       /* 0x04 */
    volatile uint32_t CFGR;        /* 0x08 */
    volatile uint32_t PLLCFGR;     /* 0x0C */
    uint32_t          RESERVED0[2]; /* 0x10-0x14 */
    volatile uint32_t CRRCR;       /* 0x18 */
    volatile uint32_t CIER;        /* 0x1C */
    volatile uint32_t CIFR;        /* 0x20 */
    volatile uint32_t CICR;        /* 0x24 */
    volatile uint32_t IOPRSTR;     /* 0x28 */
    volatile uint32_t AHBRSTR;     /* 0x2C */
    volatile uint32_t APBRSTR1;    /* 0x30 */
    volatile uint32_t APBRSTR2;    /* 0x34 */
    volatile uint32_t IOPENR;      /* 0x38 - GPIO clock enable register  <-- VERIFY THIS OFFSET CAREFULLY */
} RCC_TypeDef;

/* VERIFY: base address against RM0444 Section 2.2 memory map. */
#define RCC_BASE     (0x40021000UL)
#define RCC          ((RCC_TypeDef *)RCC_BASE)

/* VERIFY: bit position for GPIOA's enable bit in IOPENR - RM0444 RCC
 * chapter, IOPENR bit description table. */
#define RCC_IOPENR_GPIOAEN   (1U << 0)
#define RCC_IOPENR_GPIOBEN   (1U << 1)

#endif /* REG_DEFS_H */
