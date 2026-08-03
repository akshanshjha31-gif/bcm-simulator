# User Manual
### Automotive Body Control Module (BCM) Simulator

| | |
|---|---|
| **Document ID** | BCM-MAN-001 |
| **Version** | 1.0 |
| **Software version** | 1.0.0 |

---

## 1. What this is

A Body Control Module — the ECU in a car that owns the body electrics:
headlights, indicators, brake and reverse lamps, door locks, horn and power
modes. It enforces the rules a real vehicle depends on: brake light always
wins, reverse lamp only in reverse gear, never lock with a door open.

Your breadboard is a scale model of a vehicle:

| Hardware | Stands for |
|---|---|
| 6 push buttons | Driver controls — ignition, indicators, hazard, brake, lock |
| 10 LEDs | The vehicle's lamps |
| Buzzer | Horn and lock/unlock chirps |
| Potentiometer | Battery voltage — turn it down to simulate a flat battery |
| USB-UART adapter | The diagnostic port a garage plugs into |

---

## 2. One-time setup

Everything installs **per-user, no administrator rights**.

| Tool | Purpose | Install |
|---|---|---|
| arm-none-eabi-gcc | Firmware compiler | `xpm install --global @xpack-dev-tools/arm-none-eabi-gcc@latest` |
| windows-build-tools | GNU make | `xpm install --global @xpack-dev-tools/windows-build-tools@latest` |
| OpenOCD | Flash + debug over ST-Link | `xpm install --global @xpack-dev-tools/openocd@latest` |
| mingw-w64-gcc | Host tests and SIL | `xpm install --global @xpack-dev-tools/mingw-w64-gcc@latest` |
| QEMU **2.8** | Run without a board | `xpm install --global @xpack-dev-tools/qemu-arm@2.8.0-13.1` |
| .NET 8 SDK | The WPF tool | `dotnet-install.ps1 -Channel 8.0 -InstallDir "$env:USERPROFILE\.dotnet"` |
| Doxygen | API docs | Portable zip from doxygen.nl |

> **QEMU must be 2.8**, the gnuarmeclipse build. Upstream QEMU 9.x maps the
> STM32F1 RCC as a stub that always reads zero, so the firmware hangs forever
> polling `HSERDY` and gives you no clue why.

Then fetch the vendor code (STM32 HAL + FreeRTOS, not committed):

```powershell
cd firmware
make deps
```

`tools\bcm.ps1` finds all of these itself — none of them need to be on `PATH`.

---

## 3. Wiring

### 3.1 Programming — ST-Link V2

```
ST-Link          Blue Pill
3.3V      ---->  3V3
GND       ---->  GND
SWDIO     ---->  DIO   (PA13)
SWCLK     ---->  CLK   (PA14)
```

The board's power LED must be lit. If it is dark nothing else will work — see
§8.

### 3.2 Diagnostics — USB-UART adapter

```
adapter RX  <---- PB6    (BCM TX)
adapter TX  ----> PB7    (BCM RX)
adapter GND <---> GND
```

3.3 V logic, 115200 8N1. **RX and TX cross over.** Do not connect the
adapter's supply pin if the board is already powered.

> USART1 is remapped to PB6/PB7 because PA9 drives the door-lock lamp here.

### 3.3 I/O map

| Pin | Function | | Pin | Function |
|---|---|---|---|---|
| PA0 | Ignition button | | PB0 | Ignition lamp |
| PA1 | Left indicator button | | PB1 | DRL lamp |
| PA2 | Right indicator button | | PB8 | Buzzer |
| PA3 | Hazard button | | PB10 | Low beam |
| PA4 | Brake button | | PB11 | High beam |
| PA5 | Door lock button | | PB12 | Left indicator |
| PA6 | Potentiometer (ADC) | | PB13 | Right indicator |
| PA8 | Reverse lamp | | PB14 | Hazard lamp |
| PA9 | Door lock lamp | | PB15 | Brake lamp |
| PB6/PB7 | Diagnostic UART | | PA13/PA14 | SWD — leave free |

Buttons are wired `+3.3V → button → GPIO → 10 kΩ → GND` (active high).
LEDs need **220 Ω–1 kΩ**; a 10 kΩ resistor makes them invisible.

---

## 4. Everyday commands

All from the repository root.

```powershell
tools\bcm.ps1 build     # compile the firmware
tools\bcm.ps1 flash     # program the board over SWD
tools\bcm.ps1 verify    # confirm the CPU is executing
tools\bcm.ps1 test      # host unit tests
tools\bcm.ps1 sil       # scenario suite
tools\bcm.ps1 talk      # exercise the protocol over serial
tools\bcm.ps1 desktop   # build and launch the WPF tool
tools\bcm.ps1 run       # run under QEMU, no board needed
tools\bcm.ps1 debug     # QEMU halted for GDB / VS Code F5
tools\bcm.ps1 docs      # generate the Doxygen API reference
tools\bcm.ps1 ports     # list serial ports
tools\bcm.ps1 clean
```

---

## 5. Operating the BCM

After `tools\bcm.ps1 flash`:

| Action | Result |
|---|---|
| **Tap ignition** | Wake → Run; ignition lamp lights. Tap again to shut down |
| **Hold ignition ≥ 0.8 s** | Steps the lighting: Off → Parking → DRL → Low → High → Off |
| **Left / right indicator** | That lamp flashes at ~1.5 Hz. Pressing again cancels |
| **Hazard** | Both indicators plus the hazard lamp flash — and this **overrides** a running indicator |
| **Brake** | Brake lamp, at the highest priority of any lamp |
| **Door lock** | Toggles lock/unlock, chirps the buzzer, lock lamp shows state |
| **Turn the pot down** | Battery low → load shedding: DRL and high beam drop, brake and indicators stay |

The small on-board LED blinks at 1 Hz throughout. If it stops, the board has
frozen.

> Holding ignition to change the lights is a bring-up affordance. A dedicated
> light switch belongs on PB5; see `BCM_HW_EXT_IO` in `board_config.h`.

---

## 6. The diagnostic tool

```powershell
tools\bcm.ps1 desktop
```

Pick the COM port, press **Connect**. Lamps, switches, power state and battery
update live; click a lamp to drive it; lock/unlock and clear fault codes from
the Actions panel; the event log fills as things happen.

`--connect COM13` opens the link on startup.

### Two results that look wrong but are correct

- **Lock answers `NOT_PERMITTED`** — the BCM refuses to lock while a door is
  open (SRS-DOOR-002).
- **Clicking the reverse lamp leaves it dark** — host requests are merged
  *before* safety arbitration, so a diagnostic tool can exercise a lamp but
  cannot talk its way past a rule. Reverse needs reverse gear (SRS-REV-001).

---

## 7. Debugging

**In QEMU** (no board): run `tools\bcm.ps1 debug`, then press F5 in VS Code
with `firmware` open as the folder.

**On hardware**: OpenOCD exposes a GDB server on port 3333 whenever it
connects. Point `launch.json` at `localhost:3333` instead of `1234`.

---

## 8. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `unable to connect to the target` | Board unpowered, or SWDIO/SWCLK swapped, or GND missing. **Check the power LED first** |
| No LEDs light at all | Resistors too large (use 220 Ω–1 kΩ), LEDs reversed, or the ground rail is not tied to a Blue Pill GND pin |
| One LED dark, the rest fine | That LED is backwards or its wire is open |
| A button does nothing | Check 3.3 V actually reaches the GPIO when pressed, and that its 10 kΩ lands on ground |
| `no serial port found` | The USB-UART adapter is not plugged in. The **ST-Link does not carry this link** — it is a separate adapter |
| `talk` times out | RX/TX not crossed, wrong port, or wired to PA9/PA10 instead of PB6/PB7 |
| WPF: *"You must install .NET"* | The SDK is per-user, so the app host cannot find `hostfxr.dll`. Set `DOTNET_ROOT`, or use `tools\bcm.ps1 desktop` |
| Firmware hangs in QEMU | You are on QEMU 9.x. Use the 2.8 gnuarmeclipse build |
| QEMU blinks at the wrong rate | Expected — QEMU is not cycle-accurate. Hardware is exact |
| Board's micro-USB does nothing | Normal. The STM32F103 has **no USB bootloader in ROM**; that port only supplies power |

---

## 9. Repository layout

```
firmware/   drivers, services, app, bsp, core   (target)
sil/        Software-in-the-Loop harness        (host)
tests/      Catch2 unit tests                   (host)
desktop/    C# WPF diagnostic tool              (host)
docs/       SRS, architecture, ICD, UML, test report, this manual
tools/      bcm.ps1
```

---

## 10. Revision history

| Version | Change |
|---|---|
| 1.0 | Initial issue for software version 1.0.0 |
