/**
 * @file    bsp.cpp
 * @brief   Board Support Package implementation (STM32F103C8T6).
 *
 * Pin identity lives in tables, not switch-ladders, so adding a lamp is a
 * one-line table edit rather than an edit to every consumer (Open/Closed).
 */
#include "bsp.h"
#include "board_config.h"
#include "drv_adc.h"

namespace bcm {
namespace bsp {
namespace {

struct PinRef {
    GPIO_TypeDef* port;
    uint16_t      pin;
    const char*   name;
};

/* Order must match enum class Lamp. */
const PinRef kLamps[] = {
    { BCM_LAMP_IGNITION_PORT,   BCM_LAMP_IGNITION_PIN,   "Ignition"  },
    { BCM_LAMP_DRL_PORT,        BCM_LAMP_DRL_PIN,        "DRL"       },
    { BCM_LAMP_LOW_BEAM_PORT,   BCM_LAMP_LOW_BEAM_PIN,   "LowBeam"   },
    { BCM_LAMP_HIGH_BEAM_PORT,  BCM_LAMP_HIGH_BEAM_PIN,  "HighBeam"  },
    { BCM_LAMP_IND_LEFT_PORT,   BCM_LAMP_IND_LEFT_PIN,   "IndLeft"   },
    { BCM_LAMP_IND_RIGHT_PORT,  BCM_LAMP_IND_RIGHT_PIN,  "IndRight"  },
    { BCM_LAMP_HAZARD_PORT,     BCM_LAMP_HAZARD_PIN,     "Hazard"    },
    { BCM_LAMP_BRAKE_PORT,      BCM_LAMP_BRAKE_PIN,      "Brake"     },
    { BCM_LAMP_REVERSE_PORT,    BCM_LAMP_REVERSE_PIN,    "Reverse"   },
    { BCM_LAMP_DOOR_LOCK_PORT,  BCM_LAMP_DOOR_LOCK_PIN,  "DoorLock"  },
};
static_assert(sizeof(kLamps) / sizeof(kLamps[0]) ==
              static_cast<unsigned>(Lamp::Count),
              "kLamps table does not match enum class Lamp");

/* Order must match enum class Switch. */
const PinRef kSwitches[] = {
    { BCM_SW_IGNITION_PORT,   BCM_SW_IGNITION_PIN,   "Ignition"  },
    { BCM_SW_IND_LEFT_PORT,   BCM_SW_IND_LEFT_PIN,   "IndLeft"   },
    { BCM_SW_IND_RIGHT_PORT,  BCM_SW_IND_RIGHT_PIN,  "IndRight"  },
    { BCM_SW_HAZARD_PORT,     BCM_SW_HAZARD_PIN,     "Hazard"    },
    { BCM_SW_BRAKE_PORT,      BCM_SW_BRAKE_PIN,      "Brake"     },
    { BCM_SW_DOOR_LOCK_PORT,  BCM_SW_DOOR_LOCK_PIN,  "DoorLock"  },
};
static_assert(sizeof(kSwitches) / sizeof(kSwitches[0]) ==
              static_cast<unsigned>(Switch::Count),
              "kSwitches table does not match enum class Switch");

const drivers::GpioPin kHeartbeat(BCM_LED_HEARTBEAT_PORT,
                                  BCM_LED_HEARTBEAT_PIN,
                                  BCM_LED_HEARTBEAT_ACTIVE_LOW != 0);

}  // namespace

void init()
{
    BCM_GPIO_CLK_ENABLE_ALL();

    kHeartbeat.config_output();
    heartbeat_set(false);

    /* Analog pins must leave GPIO before the ADC can use them. */
    drivers::GpioPin(BCM_ADC_BATTERY_PORT, BCM_ADC_BATTERY_PIN).config_analog();
    drivers::GpioPin(BCM_ADC_AMBIENT_PORT, BCM_ADC_AMBIENT_PIN).config_analog();

    (void)drivers::Adc::init();
}

void heartbeat_set(bool on)  { kHeartbeat.write(on); }
void heartbeat_toggle()      { kHeartbeat.toggle(); }

drivers::GpioPin lamp_pin(Lamp lamp)
{
    if (lamp >= Lamp::Count) { return drivers::GpioPin(); }
    const PinRef& p = kLamps[static_cast<unsigned>(lamp)];
    return drivers::GpioPin(p.port, p.pin, BCM_LAMP_ACTIVE_LOW != 0);
}

drivers::GpioPin switch_pin(Switch sw)
{
    if (sw >= Switch::Count) { return drivers::GpioPin(); }
    const PinRef& p = kSwitches[static_cast<unsigned>(sw)];
    return drivers::GpioPin(p.port, p.pin, BCM_SW_ACTIVE_LOW != 0);
}

drivers::GpioPin buzzer_pin()
{
    return drivers::GpioPin(BCM_BUZZER_PORT, BCM_BUZZER_PIN, false);
}

uint32_t sysclk_hz()            { return BCM_SYSCLK_HZ; }
uint32_t switch_pull()          { return BCM_SW_PULL; }
uint32_t adc_battery_channel()  { return BCM_ADC_BATTERY_CHANNEL; }
uint32_t adc_ambient_channel()  { return BCM_ADC_AMBIENT_CHANNEL; }

drivers::UartConfig uart_config()
{
    drivers::UartConfig cfg;
    cfg.instance = BCM_DIAG_UART;
    cfg.baud     = BCM_DIAG_UART_BAUD;
    cfg.tx_port  = BCM_DIAG_UART_TX_PORT;
    cfg.tx_pin   = BCM_DIAG_UART_TX_PIN;
    cfg.rx_port  = BCM_DIAG_UART_RX_PORT;
    cfg.rx_pin   = BCM_DIAG_UART_RX_PIN;
    cfg.remap    = (BCM_DIAG_UART_REMAP != 0);
    return cfg;
}

const char* lamp_name(Lamp lamp)
{
    if (lamp >= Lamp::Count) { return "?"; }
    return kLamps[static_cast<unsigned>(lamp)].name;
}

}  // namespace bsp
}  // namespace bcm
