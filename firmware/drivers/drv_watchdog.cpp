/**
 * @file    drv_watchdog.cpp
 * @brief   Independent watchdog (IWDG).
 */
#include "drv_watchdog.h"

namespace bcm {
namespace drivers {

IWDG_HandleTypeDef Watchdog::handle_;
bool               Watchdog::running_ = false;

namespace {
/* LSI is nominally 40 kHz on STM32F1. With prescaler 256 the counter ticks
 * every 6.4 ms, giving a maximum window of 4096 * 6.4 ms ~= 26.2 s. */
const uint32_t kLsiHz          = 40000U;
const uint32_t kPrescalerDiv   = 256U;
const uint16_t kReloadMax      = 0x0FFFU;
}  // namespace

bool Watchdog::start(uint32_t timeout_ms)
{
    /* reload = timeout_ms * (LSI / prescaler) / 1000 */
    uint32_t reload = (timeout_ms * (kLsiHz / kPrescalerDiv)) / 1000U;
    if (reload == 0U)          { reload = 1U; }
    if (reload > kReloadMax)   { reload = kReloadMax; }

    handle_.Instance       = IWDG;
    handle_.Init.Prescaler = IWDG_PRESCALER_256;
    handle_.Init.Reload    = reload;

    if (HAL_IWDG_Init(&handle_) != HAL_OK) { return false; }

    running_ = true;
    return true;
}

void Watchdog::refresh()
{
    if (running_) { HAL_IWDG_Refresh(&handle_); }
}

bool Watchdog::reset_was_watchdog()
{
    const bool flagged = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return flagged;
}

}  // namespace drivers
}  // namespace bcm
