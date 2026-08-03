# Interface Control Document
### BCM Diagnostic Protocol over UART

| | |
|---|---|
| **Document ID** | BCM-ICD-001 |
| **Version** | 1.0 |
| **Traces** | `BCM-SRS-001` (SRS-COM-001..004), `BCM-ARC-001` §6 |
| **Applies to** | firmware `services/protocol.h`, `services/frame_codec.*`, and the C# desktop tool |

---

## 1. Purpose

This document defines the byte-level interface between the BCM firmware and any
diagnostic host (the C# WPF tool, a serial terminal, or an automated test
script). It is the **single normative reference** for the wire format — the
firmware header `services/protocol.h` and the desktop tool must both conform to
it, and any change here is a protocol version change.

---

## 2. Physical layer

| Parameter | Value |
|---|---|
| Interface | UART (USART1, **remapped**) |
| Pins | `PB6` = BCM TX, `PB7` = BCM RX |
| Baud rate | 115200 |
| Format | 8 data bits, no parity, 1 stop bit (8N1) |
| Flow control | None |
| Logic levels | 3.3 V TTL — **not** RS-232 |

> **Note on the pinout.** USART1's default pins are `PA9`/`PA10`, but `PA9`
> carries the door-lock lamp on this board. USART1 is therefore remapped to
> `PB6`/`PB7` via `AFIO_MAPR.USART1_REMAP`. The alternatives were worse:
> USART2 (`PA2`/`PA3`) collides with the indicator and hazard switches, and
> USART3 (`PB10`/`PB11`) collides with the beam lamps.

> A USB-UART adapter must be set to 3.3 V logic. Connect adapter `RX` to `PB6`,
> adapter `TX` to `PB7`, and share `GND`. Do not connect the adapter's supply
> pin if the board is already powered.

---

## 3. Frame format

Every message in both directions is a single frame:

```
+--------+--------+--------+-------------+---------+--------+
| HDR    | CMD    | LEN    | PAYLOAD     | CKSUM   | FTR    |
| 1 byte | 1 byte | 1 byte | LEN bytes   | 1 byte  | 1 byte |
| 0xAA   |        | 0..32  |             | CRC-8   | 0x55   |
+--------+--------+--------+-------------+---------+--------+
```

| Field | Description |
|---|---|
| `HDR` | Start-of-frame, always `0xAA` |
| `CMD` | Command identifier (§4). Bit 7 set = response |
| `LEN` | Payload length, `0`–`32`. Values above 32 are invalid |
| `PAYLOAD` | `LEN` bytes, command-specific |
| `CKSUM` | CRC-8 over `CMD`, `LEN` and `PAYLOAD` (§3.2) |
| `FTR` | End-of-frame, always `0x55` |

Frame size is therefore `LEN + 5`, minimum **5** bytes and maximum **37** bytes.

### 3.1 Byte transparency

Payload bytes are **not escaped**. A payload may legitimately contain `0xAA` or
`0x55`. Receivers must therefore use `LEN` to know where the payload ends, and
must validate **both** `CKSUM` and `FTR` before accepting a frame.

### 3.2 Checksum

CRC-8, computed over `CMD`, `LEN` and every payload byte — **excluding** `HDR`,
`CKSUM` and `FTR`.

| Parameter | Value |
|---|---|
| Polynomial | `0x07` (`x^8 + x^2 + x + 1`, CRC-8/ATM) |
| Initial value | `0x00` |
| Input reflected | No |
| Output reflected | No |
| Final XOR | `0x00` |
| Check value | `CRC8("123456789") == 0xF4` |

Reference implementation:

```c
uint8_t crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
        }
    }
    return crc;
}
```

CRC-8 is used rather than a XOR or additive checksum because those cannot
detect byte reordering, nor an even number of bit errors in the same bit
position — both of which occur on a noisy asynchronous line.

### 3.3 Receiver synchronisation

A receiver discards bytes until it sees `HDR`. If `LEN` exceeds 32, or `CKSUM`
mismatches, or `FTR` is not `0x55`, the frame is **rejected** and the receiver
resumes hunting for the next `HDR`.

A corrupted frame costs at most **one** additional frame — if the damaged frame
was truncated mid-payload, the following frame's header may be consumed as
payload. The link always recovers by itself; it never desynchronises
permanently.

---

## 4. Command set

Requests use the identifiers below. **A response reuses the request identifier
with bit 7 (`0x80`) set**, so a host can always correlate a reply with the
request that caused it.

| ID | Name | Request payload | Response payload |
|---|---|---|---|
| `0x01` | `PING` | 0–31 bytes, arbitrary | `STATUS`, then the request payload echoed |
| `0x02` | `GET_VERSION` | none | `STATUS`, `major`, `minor`, `patch` |
| `0x03` | `GET_STATUS` | none | `STATUS`, `lampBitmapHi`, `lampBitmapLo`, `switchBitmap`, `powerState` |
| `0x04` | `SET_LAMP` | `lampId`, `state` (0/1) | `STATUS` |
| `0x05` | `GET_LAMP` | `lampId` | `STATUS`, `state` |
| `0x06` | `DOOR_LOCK` | none | `STATUS` |
| `0x07` | `DOOR_UNLOCK` | none | `STATUS` |
| `0x08` | `GET_BATTERY` | none | `STATUS`, `permilleHi`, `permilleLo` |
| `0x09` | `GET_DTC` | none | `STATUS`, `count`, then `count` DTC bytes |
| `0x0A` | `CLEAR_DTC` | none | `STATUS` |
| `0x0B` | `RESET` | none | *(none — the BCM resets)* |
| `0x0C` | `HEARTBEAT` | none | `STATUS` |

Multi-byte integers are **big-endian** (most significant byte first).

### 4.1 Status codes

Every response begins with a status byte:

| Value | Name | Meaning |
|---|---|---|
| `0x00` | `OK` | Request accepted and executed |
| `0x01` | `UNKNOWN_CMD` | No handler registered for that identifier |
| `0x02` | `BAD_LENGTH` | Payload shorter than the command requires |
| `0x03` | `BAD_PARAMETER` | A parameter was out of range |
| `0x04` | `BUSY` | Cannot service the request right now |
| `0x05` | `NOT_PERMITTED` | Refused by a safety guard (e.g. lock with a door open) |

An unknown command is always **answered** with `UNKNOWN_CMD` rather than
ignored — silence is indistinguishable from a dead link, and the host must be
able to tell those apart.

### 4.2 Lamp identifiers

Used by `SET_LAMP` / `GET_LAMP`, and as the bit positions of the lamp bitmap in
`GET_STATUS`.

| ID | Lamp | Pin |
|---|---|---|
| `0` | Ignition | `PB0` |
| `1` | DRL | `PB1` |
| `2` | Low beam | `PB10` |
| `3` | High beam | `PB11` |
| `4` | Indicator left | `PB12` |
| `5` | Indicator right | `PB13` |
| `6` | Hazard | `PB14` |
| `7` | Brake | `PB15` |
| `8` | Reverse | `PA8` |
| `9` | Door lock | `PA9` |

### 4.3 Switch identifiers

Bit positions in the `GET_STATUS` switch bitmap.

| Bit | Switch | Pin |
|---|---|---|
| `0` | Ignition | `PA0` |
| `1` | Indicator left | `PA1` |
| `2` | Indicator right | `PA2` |
| `3` | Hazard | `PA3` |
| `4` | Brake | `PA4` |
| `5` | Door lock | `PA5` |

---

## 5. Worked examples

### 5.1 `PING` with no payload

Request:
```
AA 01 00 15 55
│  │  │  │  └── FTR
│  │  │  └───── CKSUM = crc8(01 00) = 0x15
│  │  └──────── LEN = 0
│  └─────────── CMD = PING
└────────────── HDR
```

Response (`0x01 | 0x80 = 0x81`, status OK):
```
AA 81 01 00 75 55        CKSUM = crc8(81 01 00) = 0x75
```

### 5.2 `SET_LAMP` — brake lamp on

Request (`lampId = 7`, `state = 1`):
```
AA 04 02 07 01 E2 55     CKSUM = crc8(04 02 07 01) = 0xE2
```

Response:
```
AA 84 01 00 B5 55        STATUS = OK
```

### 5.3 Rejected request

`SET_LAMP` with only one payload byte:
```
AA 04 01 07 AB 55        CKSUM = crc8(04 01 07) = 0xAB
```
Response:
```
AA 84 01 02 BB 55        STATUS = BAD_LENGTH
```

> All checksums above were generated with the reference implementation in
> §3.2 and are asserted by `tests/test_icd_examples.cpp`, so this document
> cannot silently drift from the firmware.

---

## 6. Timing and link supervision

| Parameter | Value | Note |
|---|---|---|
| Inter-byte timeout | none | Framing is length + checksum driven |
| Link-loss timeout | 2000 ms | No valid frame for this long ⇒ link declared lost |
| Response latency | ≤ 50 ms | Under nominal load |

On link loss the BCM drives outputs to a **defined safe state** rather than
holding the last commanded values (SRS-SAFETY-007). Receiving any valid frame
restores the link immediately.

---

## 7. Conformance

The firmware implementation is verified by the host unit-test suite in
`tests/`:

| Test file | Covers |
|---|---|
| `test_crc8.cpp` | Checksum, published check value, error-detection properties |
| `test_frame_codec.cpp` | Encoding layout, parsing, corruption, resynchronisation |
| `test_dispatcher.cpp` | Command routing, status codes, link supervision |

Any independent implementation — including the C# desktop tool — should
reproduce §5's worked examples byte-for-byte.

---

## 8. Revision history

| Version | Change |
|---|---|
| 1.0 | Initial issue — frame format, CRC-8, command set, status codes |
