/**
 * @file    fw_common.h
 * @brief   Project-wide primitive types, status codes and small utilities.
 *
 * Kept dependency-free (no HAL) so it is equally usable by the on-target
 * firmware and the host-side SIL / unit-test builds.
 */
#ifndef BCM_FW_COMMON_H
#define BCM_FW_COMMON_H

#include <cstdint>
#include <cstddef>

namespace bcm {

/// Canonical result type returned across module boundaries.
enum class Status : std::uint8_t {
    Ok = 0,
    Error,
    Busy,
    Timeout,
    InvalidArg,
    NotSupported,
    CrcError,
};

/// Compile-time count of a C array. Safer than sizeof/sizeof.
template <typename T, std::size_t N>
constexpr std::size_t array_size(const T (&)[N]) noexcept
{
    return N;
}

/// Clamp helper (std::clamp is C++17; the firmware targets C++14).
template <typename T>
constexpr const T &clamp(const T &v, const T &lo, const T &hi)
{
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

}  // namespace bcm

#endif /* BCM_FW_COMMON_H */
