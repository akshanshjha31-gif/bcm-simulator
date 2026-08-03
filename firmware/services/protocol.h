/**
 * @file    protocol.h
 * @brief   BCM diagnostic protocol constants (BCM-ICD-001).
 *
 * Single source of truth for the wire format. The C# desktop tool mirrors
 * this file byte-for-byte, so any change here is a change to the ICD.
 *
 * Frame layout:
 *   +------+------+------+---------+-------+------+
 *   | HDR  | CMD  | LEN  | PAYLOAD | CKSUM | FTR  |
 *   | 0xAA |      |      | LEN B   |       | 0x55 |
 *   +------+------+------+---------+-------+------+
 *
 * CKSUM is CRC-8 over CMD, LEN and PAYLOAD (see crc8.h).
 * Payload bytes are NOT escaped; the parser resynchronises on CKSUM/FTR
 * failure, which is why both a checksum and a footer are present.
 *
 * Traces SRS-COM-001..004.
 */
#ifndef BCM_PROTOCOL_H
#define BCM_PROTOCOL_H

#include <stdint.h>

namespace bcm {
namespace services {

/// Start-of-frame marker.
const uint8_t kFrameHeader = 0xAAU;

/// End-of-frame marker.
const uint8_t kFrameFooter = 0x55U;

/// Largest payload a frame may carry.
const uint8_t kMaxPayload = 32U;

/// Bytes of overhead: HDR + CMD + LEN + CKSUM + FTR.
const uint8_t kFrameOverhead = 5U;

/// Largest possible encoded frame.
const uint8_t kMaxFrameSize = kMaxPayload + kFrameOverhead;

/**
 * @brief Command identifiers (requests).
 *
 * A response reuses the request's id with kResponseFlag set, so a host can
 * always correlate a reply with what it asked for.
 */
enum class Cmd : uint8_t {
    Ping        = 0x01U,   ///< liveness check; echoes payload back
    GetVersion  = 0x02U,   ///< -> major, minor, patch
    GetStatus   = 0x03U,   ///< -> lamp bitmap, switch bitmap, power state
    SetLamp     = 0x04U,   ///< <- lamp id, state          (SRS-COM-003)
    GetLamp     = 0x05U,   ///< <- lamp id  -> state
    DoorLock    = 0x06U,   ///< lock the doors             (SRS-DOOR-001)
    DoorUnlock  = 0x07U,   ///< unlock the doors
    GetBattery  = 0x08U,   ///< -> battery level, per mille, big-endian
    GetDtc      = 0x09U,   ///< -> count, then one byte per active DTC
    ClearDtc    = 0x0AU,   ///< clear stored DTCs
    Reset       = 0x0BU,   ///< request a controlled reset
    Heartbeat   = 0x0CU,   ///< periodic liveness from the host
};

/// OR'd into the command id of a response frame.
const uint8_t kResponseFlag = 0x80U;

/**
 * @brief First payload byte of every response.
 */
enum class Status : uint8_t {
    Ok            = 0x00U,
    UnknownCmd    = 0x01U,
    BadLength     = 0x02U,
    BadParameter  = 0x03U,
    Busy          = 0x04U,
    NotPermitted  = 0x05U,   ///< refused by a safety guard
};

/// Protocol version reported by GetVersion.
const uint8_t kVersionMajor = 1U;
const uint8_t kVersionMinor = 0U;
const uint8_t kVersionPatch = 0U;

/**
 * @brief A decoded frame. Fixed storage - never heap allocated.
 */
struct Frame {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[kMaxPayload];

    Frame() : cmd(0U), len(0U), payload() {}
};

/// Build a response frame carrying just a status byte.
inline Frame make_status_response(uint8_t request_cmd, Status status)
{
    Frame f;
    f.cmd        = static_cast<uint8_t>(request_cmd | kResponseFlag);
    f.len        = 1U;
    f.payload[0] = static_cast<uint8_t>(status);
    return f;
}

}  // namespace services
}  // namespace bcm

#endif /* BCM_PROTOCOL_H */
