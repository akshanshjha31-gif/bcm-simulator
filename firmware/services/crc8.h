/**
 * @file    crc8.h
 * @brief   CRC-8 frame check sequence - pure logic, no HAL.
 *
 * Polynomial 0x07 (CRC-8/ATM), initial value 0x00, no reflection, no final
 * XOR. Chosen over a plain XOR or sum because those miss whole classes of
 * error - notably byte reordering and any even number of bit flips in the
 * same column, both of which a noisy 115200 line produces.
 *
 * Computed bitwise rather than from a lookup table: 256 bytes of flash
 * matters more here than the handful of cycles per byte, and the frames are
 * short.
 */
#ifndef BCM_CRC8_H
#define BCM_CRC8_H

#include <stdint.h>

namespace bcm {
namespace services {

class Crc8 {
public:
    /* An enum rather than `static const` members: these are prvalues, so
     * binding them to a const reference (as the unit tests do) cannot
     * ODR-use them and demand an out-of-class definition. Keeps the class
     * header-only. */
    enum : uint8_t {
        kPolynomial = 0x07U,   ///< CRC-8/ATM
        kInitial    = 0x00U
    };

    Crc8() : value_(kInitial) {}

    void reset() { value_ = kInitial; }

    void update(uint8_t byte)
    {
        value_ ^= byte;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            if ((value_ & 0x80U) != 0U) {
                value_ = static_cast<uint8_t>((value_ << 1) ^ kPolynomial);
            }
            else {
                value_ = static_cast<uint8_t>(value_ << 1);
            }
        }
    }

    void update(const uint8_t* data, uint16_t len)
    {
        if (data == 0) { return; }
        for (uint16_t i = 0U; i < len; ++i) { update(data[i]); }
    }

    uint8_t value() const { return value_; }

    /// One-shot helper.
    static uint8_t compute(const uint8_t* data, uint16_t len)
    {
        Crc8 crc;
        crc.update(data, len);
        return crc.value();
    }

private:
    uint8_t value_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_CRC8_H */
