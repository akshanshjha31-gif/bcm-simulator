/**
 * @file    uart_transport.h
 * @brief   Binds services::ITransport to the UART driver.
 *
 * Lives in the application layer because this is the composition root - the
 * only place allowed to know both the abstraction and the concrete driver
 * (architecture document section 3.1, "D").
 */
#ifndef BCM_UART_TRANSPORT_H
#define BCM_UART_TRANSPORT_H

#include "comm_mgr.h"
#include "drv_uart.h"

namespace bcm {
namespace app {

class UartTransport : public services::ITransport {
public:
    bool read_byte(uint8_t& out) override
    {
        return drivers::Uart::read(out);
    }

    bool write(const uint8_t* data, uint16_t len) override
    {
        return drivers::Uart::write(data, len);
    }
};

}  // namespace app
}  // namespace bcm

#endif /* BCM_UART_TRANSPORT_H */
