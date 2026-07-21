# Driver Layer (Phase 1)

Typed, testable wrappers over the HAL/BSP. Each driver exposes a narrow C++
interface so the Service layer can be compiled against fakes on the host.

Planned: `drv_gpio`, `drv_button` (debounce), `drv_led` (on/off/blink/pwm),
`drv_uart` (IT/DMA + ring buffers), `drv_timer`, `drv_adc`, `drv_watchdog`,
`drv_flash`. Each ships with header, source, Doxygen and unit tests.
