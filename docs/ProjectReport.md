# Project Report
### Automotive Body Control Module (BCM) Simulator — Development Record

| | |
|---|---|
| **Document ID** | BCM-PRJ-001 |
| **Version** | 1.0 |
| **Software version** | 1.0.0 |
| **Target** | STM32F103C8T6 "Blue Pill", 72 MHz Cortex-M3, 64 kB flash / 20 kB RAM |
| **Traces** | `BCM-SRS-001`, `BCM-ARC-001`, `BCM-ICD-001`, `BCM-UML-001`, `BCM-TST-001` |

---

## 1. Purpose of this document

A single record of **what was built, how it was built, which techniques were
used and why, and how to verify the whole thing**. The other documents are
specifications; this one is the development history and the operating guide
that ties them together.

Read this if you want to understand the project as a whole. Read the SRS for
requirements, the Architecture Document for design rationale, the ICD for the
wire format, and the Test Report for detailed results.

---

## 2. What the system is

A **Body Control Module** is the ECU in a vehicle that owns the body
electrics — exterior lighting, indicators, brake and reverse lamps, door
locking, horn and power modes. Its job is not merely to switch lamps but to
**enforce rules**: the brake light always wins, the reverse lamp is only legal
in reverse gear, the car never locks itself with a door open.

The hardware is a scale model of a vehicle:

| Hardware | Represents |
|---|---|
| 6 push buttons | Driver controls — ignition, indicators, hazard, brake, lock |
| 10 LEDs | The vehicle's lamps |
| Buzzer | Horn and lock/unlock chirps |
| Potentiometer | Battery voltage |
| USB-UART adapter | The OBD diagnostic port |

### 2.1 Delivered scope

| Domain | Behaviour |
|---|---|
| Exterior lighting | Off → Parking → DRL → Low → High, plus auto-headlight with hysteresis |
| Indicators | Left / right / hazard, hazard overriding a running indicator |
| Safety lamps | Brake at highest priority; reverse gated on gear |
| Doors | Lock, unlock, welcome lighting, auto-lock — never with a door open |
| Horn | Chirps on lock/unlock, rate-limited continuous sound |
| Power | Sleep → Wake → Run → Shutdown, battery supervision, load shedding |
| Comms | Framed protocol, CRC-8, command dispatch, link supervision |
| Diagnostics | POST, DTC store, health aggregation, timestamped event log |
| Safety | Central lamp arbiter, watchdog supervisor, comm-timeout safe state |

---

## 3. Development approach

Eight phases, each ending in something demonstrable. The ordering is not
arbitrary — each phase removes the largest remaining unknown:

```
0  Can we build and flash at all?          → toolchain risk
1  Can we drive the hardware reliably?     → I/O and timing risk
2  Can a PC talk to it?                    → interface risk
3  Does it behave like a BCM?              → the actual product
4  Does it behave under concurrency?       → scheduling risk
5  Can we test it without the board?       → regression cost
6  Can a user operate it?                  → usability
7  Can someone else understand it?         → maintainability
```

Two rules held throughout:

1. **Nothing is "done" until it runs on the real board.** Five of the ten
   defects found during the project were unreachable in simulation.
2. **Every layer is verified before the next is built on it.** The unit suite
   grew with each phase and never went red at a phase boundary.

---

## 4. Phase-by-phase record

### Phase 0 — Foundation

**Objective:** prove the toolchain, and write the specifications before the code.

**Built:** repository layout, layered Makefile, linker script, startup glue,
`board_config.h`, a minimal BSP, and two specifications — the SRS
(`BCM-SRS-001`) and the Architecture Document (`BCM-ARC-001`).

**Techniques**
- **Requirements-first.** Every requirement got an ID (`SRS-BRK-001`) so later
  code and tests could trace to it. This is what makes the traceability matrix
  in `BCM-TST-001` possible at all.
- **Layered architecture with a strict dependency rule** — dependencies point
  downward only. This single constraint is what later allowed most of the
  firmware to be tested on a PC.
- **Vendor code fetched, never committed** (`make deps`), keeping the
  repository to the project's own work.

**Outcome:** an image that boots, brings up the 72 MHz PLL and blinks — 3336 B.

---

### Phase 1 — Driver layer

**Objective:** reliable, testable hardware access.

**Built:** seven drivers — GPIO, button, LED, UART, timer, ADC, watchdog —
plus a host test harness.

**Technique: the split that defines the project.** Each driver is two pieces:

| HAL-free logic (host-tested) | HAL binding (target only) |
|---|---|
| `Debouncer` | `Button` |
| `Blinker` | `Led` |
| `ExpFilter`, `Hysteresis` | `Adc` |
| `RingBuffer<T,N>` | `Uart` |
| `SoftTimer` | `Watchdog` |

The left column includes only `<stdint.h>`. No HAL, no allocation, no float,
no exceptions. That is why 189 tests can run on a PC against the *same source
files* the firmware links.

**Other techniques**
- **Elapsed-time interfaces** (`update(dt_ms)`) rather than absolute ticks, so
  the same object works on target, in unit tests and in the SIL.
- **Fixed-point only** — no float on the target, for determinism and size.
- **Compile-time tables with `static_assert`** binding each table to its enum,
  so a mismatch fails the build rather than misbehaving at run time.
- **Interrupt-driven UART** with ring buffers both ways.

**Verification:** Catch2 on MinGW-w64. Tests target properties, not just happy
paths — every single-bit CRC error, ring-buffer wraparound over 1000 cycles,
saturating time deltas, and an explicit check that the debounce window fits
inside the 50 ms switch-to-lamp budget (`SRS-PERF-001`).

---

### Phase 2 — Diagnostic protocol

**Objective:** a framed protocol a PC tool can be written against independently.

**Built:** `[HDR][CMD][LEN][PAYLOAD][CKSUM][FTR]`, an incremental parser, a
table-driven dispatcher, a communication manager, and the ICD
(`BCM-ICD-001`).

**Techniques**
- **Framing separated from semantics.** `FrameCodec` knows nothing about what
  a command means; `Dispatcher` knows nothing about bytes. Each is tested in
  isolation.
- **Table-driven dispatch** — adding a command is one table entry plus a
  handler, with no existing code edited (Open/Closed).
- **CRC-8 (poly 0x07) rather than XOR.** XOR cannot detect byte reordering,
  nor an even number of bit flips in the same column — both of which a noisy
  115200 line produces. Both cases are asserted in the tests.
- **Resynchronisation by design.** Payloads are unescaped, so the receiver
  validates *both* checksum and footer and resumes hunting for a header on
  failure. A corrupt frame costs one frame, never the session.
- **Errors are answered, not ignored.** An unknown command returns
  `UNKNOWN_CMD`; silence would be indistinguishable from a dead ECU.
- **The document is executable.** `test_icd_examples.cpp` asserts every worked
  example in the ICD byte-for-byte, so the document cannot drift from the
  firmware.

---

### Phase 3 — Services and safety arbitration

**Objective:** turn "LEDs follow buttons" into a BCM.

**Built:** a shared FSM engine; the Lighting, Indicator, Door and Power state
machines; the horn manager; the fault store; and the **lamp arbiter**.

**Technique: safety expressed as data, not scattered conditionals.**

A single table-driven engine carries all four machines. Guards are table rows:

```c
{ DoorState::Unlocked, false, DoorEvent::LockRequest,
  DoorState::Locked, no_door_open, on_locked }
```

`no_door_open` sits on **every** locking transition — manual and automatic.
A rule enforced only on the manual path is not enforced. The engine also
**stops searching once a guard refuses**, so a catch-all row added later
cannot silently bypass a safety guard. That property has its own test.

**Technique: one arbiter, running last.**

```
managers publish wants → host requests merged → ARBITRATE → pins
```

`LampArbiter` is the only place that decides what actually lights, and it runs
immediately before the drivers. Rules apply in a deliberate order, with brake
**last** because it must win:

| Order | Rule | Requirement |
|---|---|---|
| 1 | Sleep blanks all but the lock lamp | SRS-PWR-003 |
| 2 | Comms lost → defined state | SRS-SAFETY-007 |
| 3 | Load shedding drops non-essential lamps | SRS-PWR-004 |
| 4 | Reverse gated on gear | SRS-REV-001 |
| 5 | **Brake overrides everything** | SRS-BRK-001 |

A consequence worth stating: `SET_LAMP` from a diagnostic host is merged
*before* arbitration, so a host can exercise a lamp but **cannot talk its way
past a safety rule**.

**Other techniques:** hysteresis on every analog threshold (so dusk cannot
make the headlights chatter), and centralised tunables in one `Config` struct
rather than literals buried in managers.

---

### Phase 4 — FreeRTOS

**Objective:** concurrency, with a watchdog that means something.

**Built:** five tasks, a static kernel configuration, and the watchdog
supervisor.

| Task | Prio | Period | Role |
|---|---|---|---|
| Watchdog | 5 | 200 ms | Refresh IWDG **only** if all tasks checked in |
| Monitor | 4 | 100 ms | Health, POST results, heartbeat |
| Control | 3 | 5 ms | Inputs, FSMs, arbitration, outputs, comms |
| Sensor | 2 | 20 ms | ADC sampling |
| Logger | 1 | queue | Drain log records to the link |

**Technique: single ownership instead of locking.** The architecture document
specifies nine tasks. Five were built: Lighting, Door and Power are merged
into Control so **every state machine has exactly one owner** and no mutex is
needed anywhere. On a single-core M3, splitting them would buy nothing and
would reintroduce the shared mutable state `SRS-REL-002` forbids. The one
cross-task datum is a `volatile uint16_t` battery reading — an aligned 16-bit
access is atomic on Cortex-M3.

**Technique: static allocation only.** `configSUPPORT_DYNAMIC_ALLOCATION` is
`0`, so no `heap_x.c` is compiled and **there is no heap at all**. "No dynamic
allocation" is enforced at link time rather than by convention, and RAM use is
exactly knowable on a 20 kB part.

**Technique: watchdog as a supervisor, not a timer.** Each task sets a bit in
an event group. The watchdog task refreshes the IWDG only when *all* bits are
set *and* health is not `Failed`, then clears them. A hung task therefore
stops the refresh and the MCU resets into a defined state (`SRS-SAFETY-006`).

Also added here: the event `Logger` (drops the **oldest** record on overflow,
because during a fault the recent events explain it) and `DiagMgr` (POST and
health, where an **untested** subsystem is explicitly *not* a pass).

---

### Phase 5 — Software-in-the-Loop

**Objective:** run whole-vehicle scenarios with no hardware.

**Technique: extract the shared core.** A SIL that tests a reimplementation
proves nothing. So the hardware-independent half of the application was
extracted into `services::BcmCore` — inputs snapshot in, arbitrated lamp and
horn state out. **The firmware and the harness link the same translation
unit.** `BcmApp` became a thin binding: `pins → Inputs → BcmCore::step() →
pins`.

**Technique: simulated time.** `SilRunner` advances time at the firmware's
real 5 ms control period, so debounce windows, blink phases and auto-lock
delays are exercised at the same granularity the target runs at — but a
30-second auto-lock scenario completes in microseconds.

**Built:** 15 scenarios / 79 checks covering interactions no unit test can
reach — brake priority *while load shedding is active*, reverse gating a
diagnostic host's own request, auto-lock refused on both paths, hysteresis at
dusk, and a full drive cycle.

---

### Phase 6 — WPF diagnostic tool

**Objective:** a usable diagnostic host, in the spirit of a garage scan tool.

**Built:** a .NET 8 WPF application — live dashboard, clickable lamps,
lock/unlock, DTC view, event feed, dark theme, MVVM with no external
framework.

**Technique: independent protocol implementation.** `Protocol/` is written
**from the ICD, not ported from the firmware**. Its xUnit suite asserts the
ICD's worked examples byte-for-byte, and `test_icd_examples.cpp` asserts the
same bytes on the firmware side. Two independent implementations agreeing on
the documented bytes is the evidence that the ICD is unambiguous — which is
the entire reason the document exists.

**Technique: correct asynchronous handling.** A background reader owns the
port; responses are matched on `request | 0x80` and everything else is
dispatched as an event. This is mandatory: unsolicited `LOG_EVENT` frames
arrive *between* a request and its reply, so a host assuming "the next frame
is my answer" desynchronises the moment the vehicle does anything.

**Technique: surface correct refusals rather than hiding them.** Lock answers
`NOT_PERMITTED` with a door open; clicking the reverse lamp leaves it dark
without reverse gear. Both are the BCM working properly, and the UI says so.

---

### Phase 7 — Documentation

**Built:** the UML model (`BCM-UML-001`), Test Report and traceability matrix
(`BCM-TST-001`), User Manual (`BCM-MAN-001`), Release Notes, and a Doxygen
API reference (360 pages, zero warnings).

**Techniques**
- **Diagrams as text.** Mermaid, so they render on GitHub and stay diffable in
  review. A binary Visio artefact drifts from the code silently.
- **Traceability matrix** mapping every SRS item → implementing module →
  verifying test.
- **Verification-by-review is labelled as such.** Two requirements cannot be
  automatically tested on this hardware and say so explicitly rather than
  being quietly claimed.
- **Limitations published, not omitted.**

---

## 5. Engineering techniques — index

| Technique | Where | Why |
|---|---|---|
| Layered architecture, downward dependencies | whole firmware | Enables host testing |
| HAL-free logic / HAL binding split | every driver, `BcmCore` | 3 of 4 test levels need no hardware |
| Table-driven FSM engine | 4 state machines | Behaviour is declarative and inspectable |
| Guards as table rows | `door_mgr`, `lighting_mgr` | A rule cannot be forgotten on one path |
| Centralised arbitration, applied last | `lamp_arbiter` | Nothing can route around a safety rule |
| Dependency injection via interfaces | `ITransport` | Comm path testable with a fake |
| Static allocation only | FreeRTOS config | No heap exists to exhaust |
| Single-owner concurrency | task design | No mutexes around any FSM |
| Event-group watchdog supervision | `rtos_app` | A hung task causes a controlled reset |
| Hysteresis on analog thresholds | `filter` | No chatter at a boundary |
| Fixed-point arithmetic only | `filter`, ADC scaling | Determinism and code size |
| CRC-8 error detection | `crc8` | Catches what XOR cannot |
| Resynchronising parser | `frame_codec` | One bad frame costs one frame |
| Executable specification | `test_icd_examples` | The ICD cannot drift |
| Two independent protocol implementations | firmware + C# | Proves the ICD is unambiguous |
| Simulated time | `SilRunner` | Long scenarios run instantly |
| `static_assert` on table/enum agreement | `bsp`, `bcm_app` | Mismatch fails the build |
| Compile-time tunables in one struct | `bcm_config` | No magic numbers |
| Crash logging in the GUI | `App.xaml.cs` | A tool that dies silently is useless |

---

## 6. Toolchain

Everything installs **per-user, no administrator rights**. `tools\bcm.ps1`
discovers each at run time, so none need to be on `PATH`.

| Tool | Version | Purpose |
|---|---|---|
| arm-none-eabi-gcc (xPack) | 15.2.1 | Firmware compiler |
| GNU make (xPack) | 4.4.1 | Build |
| OpenOCD (xPack) | 0.12.0 | Flash and debug over ST-Link |
| MinGW-w64 g++ (xPack) | 15.2.0 | Host tests and SIL |
| Catch2 | 2.13.10 | Unit test framework (fetched) |
| QEMU gnuarmeclipse | **2.8.0-13** | Run without a board |
| FreeRTOS | 11.1.0 | Kernel (fetched) |
| .NET SDK | 8.0.423 | WPF tool |
| Doxygen | 1.12.0 | API reference |

> **QEMU must be 2.8.** Upstream QEMU 9.x maps the STM32F1 RCC as a stub that
> always reads zero, so the firmware hangs polling `HSERDY` with no diagnostic.

---

## 7. How to test the complete project

Five independent levels. Levels 1–3 need **no hardware at all**.

### Level 1 — Unit tests (host)

```powershell
tools\bcm.ps1 test
```

Expected:

```
All tests passed (3630 assertions in 189 test cases)
PASS: all unit tests green.
```

Proves each module in isolation against hostile inputs. **Exits non-zero on
failure**, so it gates in CI.

---

### Level 2 — SIL scenarios (host)

```powershell
tools\bcm.ps1 sil
tools\bcm.ps1 sil -SilArgs --verbose     # list passing checks too
tools\bcm.ps1 sil -SilArgs brake-priority  # one scenario
```

Expected:

```
scenarios: 15  passed: 15  failed: 0
checks:    79  passed: 79  failed: 0
RESULT: PASS
```

Proves the assembled BCM over simulated time. Because it drives
`services::BcmCore`, a failure here is a **real defect**, not a model
disagreeing with reality.

---

### Level 3 — Interface conformance (host)

```powershell
tools\bcm.ps1 desktop -SilArgs test
```

Expected:

```
Passed! - Failed: 0, Passed: 17, Skipped: 0, Total: 17
```

Proves the C# host and the firmware agree on the documented bytes.

---

### Level 4 — Emulation (no board)

```powershell
tools\bcm.ps1 run          # free-running
tools\bcm.ps1 debug        # halted for GDB / VS Code F5
```

Proves the image boots, the clock tree comes up and the heartbeat runs.

> QEMU is **not cycle-accurate** — a 500 ms delay measures ~722 ms. Use it for
> logic, never for timing.

---

### Level 5 — Hardware

**Wiring**

```
ST-Link           Blue Pill          UART adapter      Blue Pill
3.3V     ---->    3V3                RX      <----     PB6  (BCM TX)
GND      ---->    GND                TX      ---->     PB7  (BCM RX)
SWDIO    ---->    DIO  (PA13)        GND     <--->     GND
SWCLK    ---->    CLK  (PA14)
```

The board's power LED must be lit before anything else will work.

**5a — Programme and confirm execution**

```powershell
tools\bcm.ps1 flash
tools\bcm.ps1 verify
```

Expected:

```
** Verified OK **
PASS: PC13 is toggling - firmware is running.
```

`verify` reads `GPIOC->ODR` live over SWD, so it proves the **CPU is
executing**, not merely that bytes reached flash.

**5b — Protocol over the real link**

```powershell
tools\bcm.ps1 ports      # confirm the adapter enumerated
tools\bcm.ps1 talk
```

Expected:

```
  [ OK ] PING           echo verified
  [ OK ] GET_VERSION    v1.0.0
  [ OK ] GET_STATUS     lamps=0x0000 switches=0x00
  [ OK ] GET_BATTERY    27.6% of full scale
  [ OK ] bad command    answered UNKNOWN_CMD
  [ OK ] short SETLAMP  answered BAD_LENGTH

PASS: 6/6 protocol checks OK.
```

**5c — Manual functional test**

| Action | Expected |
|---|---|
| Tap ignition | Ignition lamp on, power state → Run |
| Tap again | Lamp off, → Sleep |
| Hold ignition ≥ 0.8 s | Lighting steps Off → Parking → DRL → Low → High |
| Left / right indicator | That lamp flashes at ~1.5 Hz; pressing again cancels |
| Hazard while indicating | **Both** flash — hazard overrides |
| Press left while hazard on | Nothing changes — hazard cannot be pre-empted |
| Brake | Brake lamp, regardless of everything else |
| Door lock | Toggles, buzzer chirps, lock lamp follows |
| Turn pot down | DRL and high beam drop; **brake and indicators stay lit** |

The last row is the safety-critical one: it demonstrates load shedding
preserving the lamps other road users depend on.

**5d — Diagnostic tool**

```powershell
tools\bcm.ps1 desktop
```

Select the port, press **Connect**. Verify: firmware version appears, lamp and
switch indicators track the board, battery follows the pot, clicking a lamp
drives it, the event log fills as you press buttons.

Two results that are **correct, not faults**: lock answers `NOT_PERMITTED`
with a door open, and the reverse lamp stays dark without reverse gear.

---

### Full regression, in order

```powershell
tools\bcm.ps1 test                    # 1. units
tools\bcm.ps1 sil                     # 2. scenarios
tools\bcm.ps1 desktop -SilArgs test   # 3. interface
tools\bcm.ps1 build                   # 4. firmware compiles
tools\bcm.ps1 flash                   # 5. hardware
tools\bcm.ps1 verify
tools\bcm.ps1 talk
tools\bcm.ps1 docs                    # 6. documentation builds clean
```

---

## 8. Results

| Metric | Value |
|---|---|
| Flash | 24 564 B — **38 %** of 64 kB |
| RAM | 8 456 B bss + 124 B data — **43 %** of 20 kB |
| Unit tests | 189 cases / 3 630 assertions |
| SIL | 15 scenarios / 79 checks |
| Interface tests | 17 |
| Hardware protocol | 6/6 |
| Doxygen | 360 pages, 0 warnings |
| Modules | ~30 across four layers |

---

## 9. Defects found, and what that says about the process

Ten defects were found during development. **Five were unreachable in
simulation**, which is the argument for keeping a physical board in the loop.

| # | Defect | Found by |
|---|---|---|
| 1 | Parser statistics counters never initialised | Code review while writing tests |
| 2 | ICD checksums hand-computed and wrong | Computing them properly, then asserting them |
| 3 | Test runner reported PASS while a test failed | Noticing green with a red test |
| 4 | SysTick routed at the kernel before the scheduler existed | **GDB backtrace on target** |
| 5 | Part has 3 NVIC priority bits, not the 4 ST declares | **FreeRTOS assertion, then reading the register** |
| 6 | UART emitted truncated frames when the buffer filled | **Raw serial capture** |
| 7 | Log text interleaved with binary frames | Same capture |
| 8 | Test host assumed the next frame was its reply | Consequence of fixing 7 |
| 9 | WPF null dereference in constructor ordering | Crash log added to the app |
| 10 | WPF `TwoWay` binding onto a read-only property | Crash log |

**#5 is the most instructive.** ST's headers declare four priority bits for an
STM32F103. Writing `0xFF` to `NVIC IPR0` on this board reads back `0xE0` —
three bits. It is a clone; it also reports 128 kB of flash on a part marked
C8. The FreeRTOS port's start-up assertion caught it immediately, which is
exactly what such assertions are for.

---

## 10. Known limitations

| Limitation | Consequence |
|---|---|
| Reverse-gear, door-ajar and light-mode switches, and the parking lamp, are not wired | Forced inactive on target; exercised in SIL and over the link. Pins reserved behind `BCM_HW_EXT_IO` |
| No LDR fitted | Auto-headlight reports daylight on hardware; verified in SIL |
| `config_mgr` has no flash persistence | Tunables are compile-time defaults |
| Watchdog not fault-injected | Verified by review — `BCM-TST-001` §7.1 |
| Ignition long-press changes lighting mode | Bring-up affordance until a switch is wired to PB5 |
| QEMU timing not cycle-accurate | Logic only, never timing |

---

## 11. Document map

| Document | ID | Contents |
|---|---|---|
| Software Requirements Specification | BCM-SRS-001 | Numbered requirements |
| Architecture Document | BCM-ARC-001 | Layering, module catalogue, ADRs |
| Interface Control Document | BCM-ICD-001 | Wire format, commands, worked examples |
| UML Model | BCM-UML-001 | Class, state, sequence, deployment |
| Test Report & Traceability | BCM-TST-001 | Results, matrix, defect record |
| User Manual | BCM-MAN-001 | Setup, wiring, operation, troubleshooting |
| **Project Report** | **BCM-PRJ-001** | **This document** |
| Release Notes | — | v1.0.0 summary |
| API reference | — | `tools\bcm.ps1 docs` |

---

## 12. Revision history

| Version | Change |
|---|---|
| 1.0 | Initial issue covering software version 1.0.0 |
