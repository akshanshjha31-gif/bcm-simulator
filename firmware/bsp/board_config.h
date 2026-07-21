/**
 * @file    board_config.h
 * @brief   Board-level pin map and constants for the STM32F103C8T6 Blue Pill.
 *
 * This is the single source of truth for physical wiring. Higher layers refer
 * to logical names only (e.g. bcm::bsp), never to raw ports/pins, so re-wiring
 * or moving to another board is a change in exactly one file.
 *
 * Full BCM I/O allocation (LEDs, buttons, buzzer, ADC channels) lands in
 * Phase 1 with the driver layer; Phase 0 defines only the heartbeat LED.
 */
#ifndef BCM_BOARD_CONFIG_H
#define BCM_BOARD_CONFIG_H

#include "stm32f1xx_hal.h"

/* ---- System clock ------------------------------------------------------- */
#define BCM_SYSCLK_HZ            72000000UL   /* HSE 8 MHz * PLL9            */

/* ---- Heartbeat / status LED (on-board, PC13, active-LOW) ---------------- */
#define BCM_LED_HEARTBEAT_PORT   GPIOC
#define BCM_LED_HEARTBEAT_PIN    GPIO_PIN_13
#define BCM_LED_HEARTBEAT_CLK()  __HAL_RCC_GPIOC_CLK_ENABLE()
#define BCM_LED_HEARTBEAT_ACTIVE_LOW  1

/* ---- Diagnostic UART (reserved for Phase 2: USART1 PA9/PA10, 115200) ---- */
#define BCM_DIAG_UART            USART1
#define BCM_DIAG_UART_BAUD       115200UL

#endif /* BCM_BOARD_CONFIG_H */
