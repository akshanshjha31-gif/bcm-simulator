/**
 * @file    stm32f1xx_hal_msp.c
 * @brief   HAL MCU Support Package - low-level init hooks.
 *
 * HAL_MspInit() runs from HAL_Init(). On STM32F1 the alternate-function
 * remap logic lives in the AFIO block, so we enable its clock here; per-
 * peripheral MSP routines (UART pins, etc.) are added in later phases.
 */
#include "stm32f1xx_hal.h"

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}
