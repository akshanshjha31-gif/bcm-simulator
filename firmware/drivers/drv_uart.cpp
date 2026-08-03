/**
 * @file    drv_uart.cpp
 * @brief   Interrupt-driven UART with TX/RX ring buffers.
 *
 * HAL is used for configuration only. The interrupt path touches the
 * peripheral registers directly, because HAL_UART_IRQHandler drives a
 * transfer-oriented state machine that does not suit a continuously running
 * byte stream.
 */
#include "drv_uart.h"

namespace bcm {
namespace drivers {

UART_HandleTypeDef                       Uart::handle_;
RingBuffer<uint8_t, Uart::kTxCapacity>   Uart::tx_;
RingBuffer<uint8_t, Uart::kRxCapacity>   Uart::rx_;
bool                                     Uart::initialised_ = false;
volatile bool                            Uart::overrun_     = false;

bool Uart::init(const UartConfig& cfg)
{
    if (cfg.instance != USART1) { return false; }   /* only USART1 wired */

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* Must happen before the pins are configured: the remap decides which
     * physical pads the peripheral is wired to. */
    if (cfg.remap) { __HAL_AFIO_REMAP_USART1_ENABLE(); }

    GPIO_InitTypeDef gpio = {};

    /* TX: alternate function push-pull. */
    gpio.Pin   = cfg.tx_pin;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(cfg.tx_port, &gpio);

    /* RX: input with pull-up so an unplugged line idles high, not floating. */
    gpio.Pin  = cfg.rx_pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(cfg.rx_port, &gpio);

    handle_.Instance          = cfg.instance;
    handle_.Init.BaudRate     = cfg.baud;
    handle_.Init.WordLength   = UART_WORDLENGTH_8B;
    handle_.Init.StopBits     = UART_STOPBITS_1;
    handle_.Init.Parity       = UART_PARITY_NONE;
    handle_.Init.Mode         = UART_MODE_TX_RX;
    handle_.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    handle_.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&handle_) != HAL_OK) { return false; }

    tx_.clear();
    rx_.clear();
    overrun_ = false;

    HAL_NVIC_SetPriority(USART1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    __HAL_UART_ENABLE_IT(&handle_, UART_IT_RXNE);

    initialised_ = true;
    return true;
}

bool Uart::write(const uint8_t* data, uint16_t len)
{
    if (!initialised_ || data == 0) { return false; }

    bool ok = true;
    for (uint16_t i = 0U; i < len; ++i) {
        /* Guard the queue against the ISR draining it mid-push. */
        __HAL_UART_DISABLE_IT(&handle_, UART_IT_TXE);
        const bool pushed = tx_.push(data[i]);
        __HAL_UART_ENABLE_IT(&handle_, UART_IT_TXE);
        if (!pushed) { ok = false; break; }
    }
    return ok;
}

bool Uart::write_str(const char* text)
{
    if (text == 0) { return false; }

    uint16_t len = 0U;
    while (text[len] != '\0' && len < 0xFFFFU) { ++len; }
    return write(reinterpret_cast<const uint8_t*>(text), len);
}

bool Uart::read(uint8_t& out)
{
    if (!initialised_) { return false; }

    __HAL_UART_DISABLE_IT(&handle_, UART_IT_RXNE);
    const bool got = rx_.pop(out);
    __HAL_UART_ENABLE_IT(&handle_, UART_IT_RXNE);
    return got;
}

uint16_t Uart::rx_available() { return rx_.size(); }
uint16_t Uart::tx_pending()   { return tx_.size(); }

bool Uart::take_overrun()
{
    const bool was = overrun_;
    overrun_ = false;
    return was;
}

void Uart::on_irq()
{
    USART_TypeDef* const u = handle_.Instance;
    const uint32_t sr = u->SR;

    /* Received byte. Reading DR clears RXNE and any overrun flag. */
    if ((sr & USART_SR_RXNE) != 0U) {
        const uint8_t byte = static_cast<uint8_t>(u->DR & 0xFFU);
        if (!rx_.push(byte)) { overrun_ = true; }
    }

    /* Transmit register empty - feed the next byte or stop asking. */
    if (((sr & USART_SR_TXE) != 0U) &&
        ((u->CR1 & USART_CR1_TXEIE) != 0U)) {
        uint8_t byte = 0U;
        if (tx_.pop(byte)) {
            u->DR = byte;
        }
        else {
            __HAL_UART_DISABLE_IT(&handle_, UART_IT_TXE);
        }
    }

    /* Framing/noise/overrun: clear by reading SR then DR. */
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0U) {
        (void)u->DR;
        overrun_ = true;
    }
}

}  // namespace drivers
}  // namespace bcm

/* C entry point for the vector table (see core/stm32f1xx_it.c). */
extern "C" void bcm_usart1_irq_handler(void)
{
    bcm::drivers::Uart::on_irq();
}
