/**
 * @file    stm32f1xx_it.c
 * @brief   Cortex-M3 core exception handlers for the BCM Simulator.
 *
 * Peripheral IRQ handlers are added in later phases (UART, EXTI, timers).
 * The FreeRTOS port supplies SVC/PendSV/SysTick once the kernel is integrated
 * (Phase 4); until then SysTick drives the HAL time base here.
 */
#include "stm32f1xx_hal.h"

/* ---- Cortex-M processor faults ------------------------------------------ */

void NMI_Handler(void)
{
    while (1) { }
}

void HardFault_Handler(void)
{
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void DebugMon_Handler(void) { }

/* SVC_Handler, PendSV_Handler and SysTick_Handler are supplied by the
 * FreeRTOS port (see FreeRTOSConfig.h, which maps the kernel's handler names
 * onto these vector-table entries). Defining them here too would produce a
 * duplicate-symbol error at link time.
 *
 * The HAL still needs its millisecond tick, which is fed from
 * vApplicationTickHook in app/rtos_app.cpp. */

/* ---- Peripheral IRQs ----------------------------------------------------- */

/* Defined in drivers/drv_uart.cpp (extern "C"). Kept as a plain C symbol so
 * this translation unit stays free of C++ headers. */
extern void bcm_usart1_irq_handler(void);

void USART1_IRQHandler(void)
{
    bcm_usart1_irq_handler();
}
