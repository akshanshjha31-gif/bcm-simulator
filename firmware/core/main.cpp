/**
 * @file    main.cpp
 * @brief   BCM Simulator firmware entry point (Phase 0 bring-up).
 *
 * Phase 0 proves the full toolchain path: startup -> C++ static init ->
 * HAL init -> 72 MHz clock tree -> GPIO -> timed heartbeat. Later phases
 * replace the bare super-loop with the FreeRTOS scheduler and the BCM
 * service/state-machine stack.
 */
#include "stm32f1xx_hal.h"
#include "bsp.h"
#include "rtos_app.h"

namespace {

/**
 * @brief Configure the clock tree for 72 MHz operation.
 *
 * Blue Pill has an 8 MHz HSE crystal:
 *   SYSCLK = HSE(8 MHz) x PLL(9)      = 72 MHz
 *   HCLK   = SYSCLK / 1               = 72 MHz
 *   PCLK1  = HCLK   / 2               = 36 MHz   (APB1 max)
 *   PCLK2  = HCLK   / 1               = 72 MHz
 * Flash requires 2 wait states at 72 MHz.
 */
void SystemClock_Config()
{
    RCC_OscInitTypeDef osc = {};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1) { }
    }

    RCC_ClkInitTypeDef clk = {};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        while (1) { }
    }
}

}  // namespace

int main()
{
    HAL_Init();
    SystemClock_Config();

    bcm::bsp::init();

    /* Phase 4: hand control to FreeRTOS. The task set, the watchdog
     * supervisor and the kernel hooks live in app/rtos_app.cpp; the service
     * layer is unchanged from Phase 3, which is the point of the layering. */
    bcm::app::rtos_start();

    for (;;) { }   /* unreachable - rtos_start() does not return */
}
