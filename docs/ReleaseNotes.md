# Release Notes

## v1.0.0 — Automotive Body Control Module (BCM) Simulator

First complete release. All eight phases of the roadmap are done, and the
firmware runs on a physical STM32F103C8.

---

### At a glance

| | |
|---|---|
| Target | STM32F103C8 "Blue Pill", 72 MHz Cortex-M3 |
| Firmware | C++14, FreeRTOS 11.1.0, **24.6 kB flash (38 %)**, **8.5 kB RAM (43 %)** |
| Protocol | BCM-ICD-001 v1.0 over UART, 115200 8N1 |
| Desktop | C# / WPF, .NET 8, MVVM |
| Verification | 189 unit tests · 15 SIL scenarios · 17 interface tests · hardware |

---

### What it does

| Domain | Behaviour |
|---|---|
| Exterior lighting | Off → Parking → DRL → Low beam → High beam, plus auto-headlight with hysteresis |
| Indicators | Left, right, hazard — hazard overrides a running indicator |
| Safety lamps | Brake (highest priority of any lamp), reverse (gear-gated) |
| Doors | Lock, unlock, welcome lighting, auto-lock — **never with a door open** |
| Horn | Buzzer with lock/unlock chirps and a rate limit |
| Power | Sleep → Wake → Run → Shutdown, battery supervision, load shedding |
| Comms | Framed protocol with CRC-8, command dispatch, link supervision |
| Diagnostics | POST, DTC store, health aggregation, timestamped event log |
| Safety | Central lamp arbiter, watchdog supervisor, comm-timeout safe state |

---

### Design decisions worth knowing

**Safety rules live in one place.** `LampArbiter` runs **last**, immediately
before anything reaches a pin. Neither a feature nor a diagnostic host can
route around it — `SET_LAMP` requests are merged *before* arbitration, so a
host can exercise a lamp but cannot defeat a rule.

**The open-door rule is an FSM guard, not an `if`.** It therefore covers the
automatic lock path as well as the manual one. The FSM engine also stops
searching once a guard refuses, so a catch-all row added later cannot silently
bypass it — there is a test for exactly that.

**CRC-8 rather than a XOR checksum.** XOR cannot detect byte reordering, nor an
even number of bit flips in the same column — both of which a noisy 115200 line
produces. Both cases are asserted.

**Every driver is split in two** — hardware-free logic and a thin HAL binding.
That is what lets 189 unit tests and 15 SIL scenarios run natively against the
*same source files* the firmware links.

**No heap anywhere.** `configSUPPORT_DYNAMIC_ALLOCATION` is 0, so no
`heap_x.c` is compiled at all and every kernel object is statically allocated.
"No dynamic allocation" is enforced at link time rather than by convention.

**Five tasks, not the nine the architecture document lists.** Lighting, Door
and Power are merged into Control so every state machine has a single owner and
needs no mutex. On a single-core M3 splitting them would buy nothing and would
reintroduce the shared mutable state SRS-REL-002 forbids.

---

### Hardware notes for this board

- **USART1 is remapped to PB6/PB7.** PA9 drives the door-lock lamp here.
  USART2 collides with the indicator switches, USART3 with the beam lamps.
  The ROM bootloader is fixed to PA9/PA10 and cannot be remapped.
- **This part implements 3 NVIC priority bits, not 4.** ST's headers declare
  four for an F103; writing `0xFF` to `NVIC IPR0` reads back `0xE0`. The board
  also reports 128 kB of flash on a part marked C8 — both are clone tells.
  `configPRIO_BITS` is 3, which is also valid on genuine silicon.
- **QEMU must be the gnuarmeclipse 2.8 build.** Upstream QEMU 9.x stubs the
  STM32F1 RCC, so the firmware hangs polling `HSERDY` with no diagnostic.

---

### Known limitations

| Limitation | Consequence |
|---|---|
| Reverse-gear, door-ajar and light-mode switches, and the parking lamp, are not wired | Forced inactive on target; exercised in SIL and over the diagnostic link. Pins reserved behind `BCM_HW_EXT_IO` |
| No LDR fitted | Auto-headlight reports full daylight on hardware; verified in SIL |
| `config_mgr` has no flash persistence | Tunables are compile-time defaults |
| Watchdog supervision not fault-injected | Verified by review — see BCM-TST-001 §7.1 |
| Holding ignition changes the lighting mode | A bring-up affordance until a light switch is wired to PB5 |
| QEMU timing is not cycle-accurate | 722 ms measured for a 500 ms delay; hardware is exact |

---

### Documentation

| Document | ID |
|---|---|
| Software Requirements Specification | BCM-SRS-001 |
| Architecture Document | BCM-ARC-001 |
| Interface Control Document | BCM-ICD-001 |
| UML Model | BCM-UML-001 |
| Test Report & Traceability | BCM-TST-001 |
| User Manual | BCM-MAN-001 |
| API reference | `tools\bcm.ps1 docs` → `docs/api/html/index.html` |

---

### Next

Ordered by value rather than effort:

1. Wire the four missing inputs, then enable `BCM_HW_EXT_IO` — this removes
   the largest gap between what is tested and what is wired.
2. Flash persistence in `config_mgr`, so tunables survive a reset.
3. Fault-inject the watchdog to promote SRS-SAFETY-006 from review to test.
4. CI: the unit tests, SIL and interface tests all exit non-zero on failure
   and need no hardware, so they gate cleanly.
5. CAN instead of UART, if the project ever wants to be closer to a real
   vehicle bus (ADR-001 chose UART deliberately for cost).
