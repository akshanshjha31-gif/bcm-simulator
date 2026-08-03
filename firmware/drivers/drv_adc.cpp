/**
 * @file    drv_adc.cpp
 * @brief   ADC1 bring-up and single-channel conversion.
 */
#include "drv_adc.h"

namespace bcm {
namespace drivers {

ADC_HandleTypeDef Adc::handle_;
bool              Adc::initialised_ = false;

bool Adc::init()
{
    if (initialised_) { return true; }

    /* ADCCLK must not exceed 14 MHz; PCLK2 is 72 MHz, so divide by 6 = 12 MHz. */
    RCC_PeriphCLKInitTypeDef periph = {};
    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection    = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) { return false; }

    __HAL_RCC_ADC1_CLK_ENABLE();

    handle_.Instance                   = ADC1;
    handle_.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    handle_.Init.ContinuousConvMode    = DISABLE;
    handle_.Init.DiscontinuousConvMode = DISABLE;
    handle_.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    handle_.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    handle_.Init.NbrOfConversion       = 1;
    if (HAL_ADC_Init(&handle_) != HAL_OK) { return false; }

    if (HAL_ADCEx_Calibration_Start(&handle_) != HAL_OK) { return false; }

    initialised_ = true;
    return true;
}

uint16_t Adc::read_raw(uint32_t channel)
{
    if (!initialised_) { return 0U; }

    ADC_ChannelConfTypeDef ch = {};
    ch.Channel      = channel;
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&handle_, &ch) != HAL_OK) { return 0U; }

    if (HAL_ADC_Start(&handle_) != HAL_OK) { return 0U; }

    uint16_t value = 0U;
    if (HAL_ADC_PollForConversion(&handle_, 10U) == HAL_OK) {
        value = static_cast<uint16_t>(HAL_ADC_GetValue(&handle_));
    }
    HAL_ADC_Stop(&handle_);
    return value;
}

}  // namespace drivers
}  // namespace bcm
