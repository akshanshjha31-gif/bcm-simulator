/**
 * @file    drv_uart.h
 * @brief   Interrupt-driven UART with TX/RX ring buffers.
 *
 * Both directions are buffered so neither the caller nor the ISR ever blocks:
 * write() queues bytes and enables the TXE interrupt, which drains the queue;
 * RXNE pushes received bytes into the RX queue for the Comm task to collect
 * (SRS-COM-001).
 *
 * Pins are passed in by the BSP rather than read from board_config.h here -
 * the driver layer must not depend on board wiring.
 */
#ifndef BCM_DRV_UART_H
#define BCM_DRV_UART_H

#include "stm32f1xx_hal.h"
#include "ring_buffer.h"
#include <stdint.h>

namespace bcm {
namespace drivers {

struct UartConfig {
    USART_TypeDef* instance;
    uint32_t       baud;
    GPIO_TypeDef*  tx_port;
    uint16_t       tx_pin;
    GPIO_TypeDef*  rx_port;
    uint16_t       rx_pin;
    /// Move USART1 off its default PA9/PA10 pins onto PB6/PB7 (AFIO_MAPR).
    bool           remap;

    UartConfig()
        : instance(0), baud(0U), tx_port(0), tx_pin(0U),
          rx_port(0), rx_pin(0U), remap(false)
    {
    }
};

class Uart {
public:
    static const uint16_t kTxCapacity = 128U;
    static const uint16_t kRxCapacity = 128U;

    /// Bring up the peripheral, its pins and the RX interrupt.
    static bool init(const UartConfig& cfg);

    /// Queue bytes for transmission. False if the TX buffer overflowed.
    static bool write(const uint8_t* data, uint16_t len);

    /// Queue a NUL-terminated string.
    static bool write_str(const char* text);

    /// Pop one received byte. False if none available.
    static bool read(uint8_t& out);

    static uint16_t rx_available();
    static uint16_t tx_pending();

    /// True if any received byte was dropped because the RX buffer was full.
    /// Cleared by reading. Surfaces as a DTC in Phase 2.
    static bool take_overrun();

    static bool ready() { return initialised_; }

    /// Called from USART1_IRQHandler. Not for application use.
    static void on_irq();

private:
    static UART_HandleTypeDef              handle_;
    static RingBuffer<uint8_t, kTxCapacity> tx_;
    static RingBuffer<uint8_t, kRxCapacity> rx_;
    static bool                             initialised_;
    static volatile bool                    overrun_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_UART_H */
