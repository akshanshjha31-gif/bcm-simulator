/**
 * @file    drv_gpio.cpp
 * @brief   Typed digital I/O over the STM32 HAL.
 */
#include "drv_gpio.h"

namespace bcm {
namespace drivers {

void GpioPin::config_output() const
{
    if (!valid()) { return; }

    GPIO_InitTypeDef cfg = {};
    cfg.Pin   = pin_;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    cfg.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(port_, &cfg);

    write(false);
}

void GpioPin::config_input(uint32_t pull) const
{
    if (!valid()) { return; }

    GPIO_InitTypeDef cfg = {};
    cfg.Pin  = pin_;
    cfg.Mode = GPIO_MODE_INPUT;
    cfg.Pull = pull;
    HAL_GPIO_Init(port_, &cfg);
}

void GpioPin::config_analog() const
{
    if (!valid()) { return; }

    GPIO_InitTypeDef cfg = {};
    cfg.Pin  = pin_;
    cfg.Mode = GPIO_MODE_ANALOG;
    cfg.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port_, &cfg);
}

void GpioPin::write(bool asserted) const
{
    if (!valid()) { return; }

    const bool level = active_low_ ? !asserted : asserted;
    HAL_GPIO_WritePin(port_, pin_, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool GpioPin::read() const
{
    if (!valid()) { return false; }

    const bool level = (HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET);
    return active_low_ ? !level : level;
}

void GpioPin::toggle() const
{
    if (!valid()) { return; }
    HAL_GPIO_TogglePin(port_, pin_);
}

}  // namespace drivers
}  // namespace bcm
