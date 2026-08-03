/**
 * @file    board_config.h
 * @brief   Board-level pin map and constants for the STM32F103C8T6 Blue Pill.
 *
 * This is the single source of truth for physical wiring. Higher layers refer
 * to logical names only (e.g. bcm::bsp), never to raw ports/pins, so re-wiring
 * or moving to another board is a change in exactly one file.
 *
 * Reserved by the debug probe - never allocate these:
 *   PA13 = SWDIO, PA14 = SWCLK  (ST-Link V2)
 *   PB2  = BOOT1, PB3 = JTDO, PB4 = NJTRST (need a JTAG remap to use as GPIO)
 */
#ifndef BCM_BOARD_CONFIG_H
#define BCM_BOARD_CONFIG_H

#include "stm32f1xx_hal.h"

/* ---- System clock ------------------------------------------------------- */
#define BCM_SYSCLK_HZ            72000000UL   /* HSE 8 MHz * PLL9            */

/* ---- Port clocks -------------------------------------------------------- */
#define BCM_GPIO_CLK_ENABLE_ALL()                 \
    do {                                          \
        __HAL_RCC_GPIOA_CLK_ENABLE();             \
        __HAL_RCC_GPIOB_CLK_ENABLE();             \
        __HAL_RCC_GPIOC_CLK_ENABLE();             \
    } while (0)

/* ==========================================================================
 *  Signal polarity
 *
 *  Switches: wired as
 *
 *      +3V3 ---- button ----+---- GPIO
 *                           |
 *                          10k
 *                           |
 *                          GND
 *
 *  i.e. an EXTERNAL pull-down. Released = pin held at GND = LOW;
 *  pressed = pin tied to 3V3 = HIGH. So the switches are active HIGH, and the
 *  internal pull-up/pull-down must stay OFF or it would fight the 10k.
 *
 *  Lamps: discrete LEDs driven high through a series resistor to GND, so
 *  active HIGH. The on-board PC13 LED is the exception - it sinks to the 3V3
 *  rail and is therefore active LOW.
 * ========================================================================== */
#define BCM_SW_ACTIVE_LOW             0
#define BCM_SW_PULL                   GPIO_NOPULL   /* external 10k pull-down */
#define BCM_LAMP_ACTIVE_LOW           0

/* ---- Heartbeat / status LED (on-board, PC13, active-LOW) ---------------- */
#define BCM_LED_HEARTBEAT_PORT   GPIOC
#define BCM_LED_HEARTBEAT_PIN    GPIO_PIN_13
#define BCM_LED_HEARTBEAT_CLK()  __HAL_RCC_GPIOC_CLK_ENABLE()
#define BCM_LED_HEARTBEAT_ACTIVE_LOW  1

/* ==========================================================================
 *  Switch inputs (push buttons)                          [SRS-SENS-002]
 * ========================================================================== */
#define BCM_SW_IGNITION_PORT     GPIOA
#define BCM_SW_IGNITION_PIN      GPIO_PIN_0    /* also WKUP - used by Power FSM */

#define BCM_SW_IND_LEFT_PORT     GPIOA
#define BCM_SW_IND_LEFT_PIN      GPIO_PIN_1

#define BCM_SW_IND_RIGHT_PORT    GPIOA
#define BCM_SW_IND_RIGHT_PIN     GPIO_PIN_2

#define BCM_SW_HAZARD_PORT       GPIOA
#define BCM_SW_HAZARD_PIN        GPIO_PIN_3

#define BCM_SW_BRAKE_PORT        GPIOA
#define BCM_SW_BRAKE_PIN         GPIO_PIN_4

#define BCM_SW_DOOR_LOCK_PORT    GPIOA
#define BCM_SW_DOOR_LOCK_PIN     GPIO_PIN_5

/* ==========================================================================
 *  Analog inputs                                          [SRS-SENS-001]
 * ========================================================================== */
#define BCM_ADC_BATTERY_PORT     GPIOA
#define BCM_ADC_BATTERY_PIN      GPIO_PIN_6
#define BCM_ADC_BATTERY_CHANNEL  ADC_CHANNEL_6   /* potentiometer = V_batt    */

#define BCM_ADC_AMBIENT_PORT     GPIOA
#define BCM_ADC_AMBIENT_PIN      GPIO_PIN_7
#define BCM_ADC_AMBIENT_CHANNEL  ADC_CHANNEL_7   /* LDR = ambient light       */

/* ==========================================================================
 *  Lamp outputs                                    [SRS-LIGHT-*, IND-*, ...]
 * ========================================================================== */
#define BCM_LAMP_REVERSE_PORT    GPIOA
#define BCM_LAMP_REVERSE_PIN     GPIO_PIN_8

/* PA9 is the USART1 TX pin in its default mapping. It carries the door-lock
 * lamp on this board instead, so USART1 is remapped to PB6/PB7 below. */
#define BCM_LAMP_DOOR_LOCK_PORT  GPIOA
#define BCM_LAMP_DOOR_LOCK_PIN   GPIO_PIN_9

#define BCM_LAMP_IGNITION_PORT   GPIOB
#define BCM_LAMP_IGNITION_PIN    GPIO_PIN_0

#define BCM_LAMP_DRL_PORT        GPIOB
#define BCM_LAMP_DRL_PIN         GPIO_PIN_1

#define BCM_LAMP_LOW_BEAM_PORT   GPIOB
#define BCM_LAMP_LOW_BEAM_PIN    GPIO_PIN_10

#define BCM_LAMP_HIGH_BEAM_PORT  GPIOB
#define BCM_LAMP_HIGH_BEAM_PIN   GPIO_PIN_11

#define BCM_LAMP_IND_LEFT_PORT   GPIOB
#define BCM_LAMP_IND_LEFT_PIN    GPIO_PIN_12

#define BCM_LAMP_IND_RIGHT_PORT  GPIOB
#define BCM_LAMP_IND_RIGHT_PIN   GPIO_PIN_13

#define BCM_LAMP_HAZARD_PORT     GPIOB
#define BCM_LAMP_HAZARD_PIN      GPIO_PIN_14

#define BCM_LAMP_BRAKE_PORT      GPIOB
#define BCM_LAMP_BRAKE_PIN       GPIO_PIN_15

/* ---- Horn / buzzer (PB8 = TIM4_CH3, so PWM tone is possible) ------------ */
#define BCM_BUZZER_PORT          GPIOB
#define BCM_BUZZER_PIN           GPIO_PIN_8

/* ---- Diagnostic UART (USART1 REMAPPED to PB6/PB7, 115200 8N1) -----------
 *
 * The default USART1 mapping is PA9/PA10, but PA9 drives the door-lock lamp
 * on this board. The STM32F103 can move USART1 to PB6 (TX) / PB7 (RX) via
 * AFIO_MAPR.USART1_REMAP, and both pins are otherwise unused here.
 *
 * The alternatives were worse: USART2 (PA2/PA3) collides with the indicator
 * and hazard switches, and USART3 (PB10/PB11) collides with the beam lamps.
 */
#define BCM_DIAG_UART            USART1
#define BCM_DIAG_UART_BAUD       115200UL
#define BCM_DIAG_UART_REMAP      1            /* AFIO remap to PB6/PB7 */
#define BCM_DIAG_UART_TX_PORT    GPIOB
#define BCM_DIAG_UART_TX_PIN     GPIO_PIN_6
#define BCM_DIAG_UART_RX_PORT    GPIOB
#define BCM_DIAG_UART_RX_PIN     GPIO_PIN_7

/* ==========================================================================
 *  NOT YET WIRED - proposed allocation for SRS functions that currently have
 *  no physical pin. PB6/PB7 are now taken by the remapped USART1, leaving
 *  PB5, PB9, PA11 and PA12 free. Guarded so nothing is driven until the
 *  hardware actually exists: define BCM_HW_EXT_IO=1 once wired.
 *
 *    SRS-LIGHT-001/004  Parking lamp  - Lighting FSM state, must be separate
 *    SRS-LIGHT-001      Light-mode switch - cycles OFF->Park->DRL->Low->High
 *    SRS-REV-001        Reverse gear switch - lamp must be gear-gated
 *    SRS-DOOR-002       Door-ajar switch - blocks auto-lock (safety invariant)
 * ========================================================================== */
#ifndef BCM_HW_EXT_IO
#define BCM_HW_EXT_IO            0
#endif

#if BCM_HW_EXT_IO
#define BCM_LAMP_PARKING_PORT    GPIOB
#define BCM_LAMP_PARKING_PIN     GPIO_PIN_9

#define BCM_SW_LIGHT_MODE_PORT   GPIOB
#define BCM_SW_LIGHT_MODE_PIN    GPIO_PIN_5

#define BCM_SW_REVERSE_PORT      GPIOA
#define BCM_SW_REVERSE_PIN       GPIO_PIN_11

#define BCM_SW_DOOR_AJAR_PORT    GPIOA
#define BCM_SW_DOOR_AJAR_PIN     GPIO_PIN_12
#endif /* BCM_HW_EXT_IO */

#endif /* BCM_BOARD_CONFIG_H */
