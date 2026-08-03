/**
 * @file    selftest.cpp
 * @brief   Board bring-up self-test - a wiring diagnostic, not the BCM.
 *
 * Superseded as the main application by BcmApp (Phase 3), but kept because it
 * is the fastest way to isolate a hardware fault: it exercises pins directly,
 * with no state machines or arbitration that could mask a bad connection.
 *
 * To use it, call selftest_run() from main() instead of BcmApp.
 *
 * Stage 1 drives every lamp pin HIGH then LOW, so LEDs blink whichever way
 *   round they are wired. A lamp dark through both phases is a real fault.
 * Stage 2 mirrors each switch onto its lamp, plus the potentiometer.
 */
#include "selftest.h"
#include "bsp.h"
#include "drv_adc.h"
#include "drv_button.h"
#include "drv_led.h"
#include "drv_timer.h"
#include "drv_uart.h"

namespace bcm {
namespace app {
namespace {

using drivers::Button;
using drivers::DeltaClock;
using drivers::Led;

struct Mirror {
    bsp::Switch sw;
    bsp::Lamp   lamp;
};

const Mirror kMirror[] = {
    { bsp::Switch::Ignition, bsp::Lamp::Ignition },
    { bsp::Switch::IndLeft,  bsp::Lamp::IndLeft  },
    { bsp::Switch::IndRight, bsp::Lamp::IndRight },
    { bsp::Switch::Hazard,   bsp::Lamp::Hazard   },
    { bsp::Switch::Brake,    bsp::Lamp::Brake    },
    { bsp::Switch::DoorLock, bsp::Lamp::DoorLock },
};
const unsigned kMirrorCount = sizeof(kMirror) / sizeof(kMirror[0]);

const uint16_t kHeartbeatMs = 500U;
const uint16_t kStageOneMs  = 1500U;
const uint16_t kLoopMs      = 5U;

/* 12-bit ADC scaled to per mille. */
const uint16_t kPotThird     = 333U;
const uint16_t kPotTwoThirds = 667U;

Led    g_lamps[static_cast<unsigned>(bsp::Lamp::Count)];
Button g_buttons[static_cast<unsigned>(bsp::Switch::Count)];

void build_drivers()
{
    for (unsigned i = 0U; i < static_cast<unsigned>(bsp::Lamp::Count); ++i) {
        g_lamps[i] = Led(bsp::lamp_pin(static_cast<bsp::Lamp>(i)));
        g_lamps[i].init();
    }
    for (unsigned i = 0U; i < static_cast<unsigned>(bsp::Switch::Count); ++i) {
        g_buttons[i] = Button(bsp::switch_pin(static_cast<bsp::Switch>(i)));
        g_buttons[i].init(bsp::switch_pull());
    }
}

}  // namespace

void selftest_lamps_on()
{
    build_drivers();
    const drivers::GpioPin buzzer = bsp::buzzer_pin();
    buzzer.config_output();
    buzzer.write(false);

    DeltaClock clock;
    bool       level   = true;
    uint16_t   elapsed = 0U;
    uint16_t   hb      = 0U;

    for (;;) {
        const uint16_t dt = clock.tick();
        elapsed = static_cast<uint16_t>(elapsed + dt);
        hb      = static_cast<uint16_t>(hb + dt);

        if (elapsed >= kStageOneMs) {
            elapsed = 0U;
            level   = !level;
            for (unsigned i = 0U; i < static_cast<unsigned>(bsp::Lamp::Count); ++i) {
                g_lamps[i].set(level);
            }
        }
        if (hb >= kHeartbeatMs) {
            hb = 0U;
            bsp::heartbeat_toggle();
        }
        drivers::Tick::delay_ms(kLoopMs);
    }
}

void selftest_mirror()
{
    build_drivers();

    const drivers::GpioPin buzzer = bsp::buzzer_pin();
    buzzer.config_output();
    buzzer.write(false);

    drivers::AnalogInput battery(bsp::adc_battery_channel());

    drivers::Uart::init(bsp::uart_config());
    drivers::Uart::write_str("\r\nBCM self-test: press a button\r\n");

    DeltaClock clock;
    uint16_t   hb = 0U;

    for (;;) {
        const uint16_t dt = clock.tick();

        for (unsigned i = 0U; i < static_cast<unsigned>(bsp::Switch::Count); ++i) {
            g_buttons[i].update(dt);
        }

        for (unsigned i = 0U; i < kMirrorCount; ++i) {
            const Mirror& m   = kMirror[i];
            Button&       btn = g_buttons[static_cast<unsigned>(m.sw)];
            Led&          led = g_lamps[static_cast<unsigned>(m.lamp)];

            if (btn.just_pressed()) {
                led.on();
                drivers::Uart::write_str(bsp::lamp_name(m.lamp));
                drivers::Uart::write_str(" ON\r\n");
            }
            else if (btn.just_released()) {
                led.off();
                drivers::Uart::write_str(bsp::lamp_name(m.lamp));
                drivers::Uart::write_str(" off\r\n");
            }
        }

        buzzer.write(g_buttons[static_cast<unsigned>(bsp::Switch::DoorLock)].pressed());

        battery.sample();
        const uint16_t level = battery.permille();
        g_lamps[static_cast<unsigned>(bsp::Lamp::Drl)].set(level > kPotThird);
        g_lamps[static_cast<unsigned>(bsp::Lamp::HighBeam)].set(level > kPotTwoThirds);

        for (unsigned i = 0U; i < static_cast<unsigned>(bsp::Lamp::Count); ++i) {
            g_lamps[i].update(dt);
        }

        hb = static_cast<uint16_t>(hb + dt);
        if (hb >= kHeartbeatMs) {
            hb = 0U;
            bsp::heartbeat_toggle();
        }
        drivers::Tick::delay_ms(kLoopMs);
    }
}

void selftest_run() { selftest_mirror(); }

}  // namespace app
}  // namespace bcm
