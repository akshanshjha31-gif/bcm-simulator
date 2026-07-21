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

void SVC_Handler(void)      { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void)   { }

/* ---- System tick: HAL time base ----------------------------------------- */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
