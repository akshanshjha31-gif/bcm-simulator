/**
 * @file    bsp.cpp
 * @brief   Board Support Package implementation (STM32F103C8T6).
 */
#include "bsp.h"
#include "board_config.h"

namespace bcm {
namespace bsp {

void init()
{
    BCM_LED_HEARTBEAT_CLK();

    GPIO_InitTypeDef cfg = {};
    cfg.Pin   = BCM_LED_HEARTBEAT_PIN;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    cfg.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(BCM_LED_HEARTBEAT_PORT, &cfg);

    heartbeat_set(false);
}

void heartbeat_set(bool on)
{
#if BCM_LED_HEARTBEAT_ACTIVE_LOW
    const GPIO_PinState level = on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#else
    const GPIO_PinState level = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#endif
    HAL_GPIO_WritePin(BCM_LED_HEARTBEAT_PORT, BCM_LED_HEARTBEAT_PIN, level);
}

void heartbeat_toggle()
{
    HAL_GPIO_TogglePin(BCM_LED_HEARTBEAT_PORT, BCM_LED_HEARTBEAT_PIN);
}

}  // namespace bsp
}  // namespace bcm
