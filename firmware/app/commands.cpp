/**
 * @file    commands.cpp
 * @brief   Concrete command handlers (BCM-ICD-001 section 4).
 */
#include "commands.h"
#include "protocol.h"

namespace bcm {
namespace app {
namespace {

using services::Cmd;
using services::CommandEntry;
using services::Frame;
using services::LampDrive;
using services::LampId;
using services::Status;

CommandContext* ctx_of(void* context)
{
    return static_cast<CommandContext*>(context);
}

/* 0x01 PING - echo the payload back after the status byte. */
bool handle_ping(const Frame& request, Frame& response, void*)
{
    response.len = static_cast<uint8_t>(1U + request.len);
    for (uint8_t i = 0U; i < request.len; ++i) {
        response.payload[1U + i] = request.payload[i];
    }
    return true;
}

/* 0x02 GET_VERSION -> major, minor, patch. */
bool handle_get_version(const Frame&, Frame& response, void*)
{
    response.len        = 4U;
    response.payload[1] = services::kVersionMajor;
    response.payload[2] = services::kVersionMinor;
    response.payload[3] = services::kVersionPatch;
    return true;
}

/* 0x03 GET_STATUS -> lamp bitmap (16 bit), switch bitmap, power state. */
bool handle_get_status(const Frame&, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);

    const uint16_t lamps = (c != 0 && c->actual != 0) ? c->actual->bitmap() : 0U;

    response.len        = 5U;
    response.payload[1] = static_cast<uint8_t>((lamps >> 8) & 0xFFU);
    response.payload[2] = static_cast<uint8_t>(lamps & 0xFFU);
    response.payload[3] = (c != 0) ? c->switch_bitmap : 0U;
    response.payload[4] = (c != 0) ? c->power_state : 0U;
    return true;
}

/* 0x04 SET_LAMP <- lampId, state.
 *
 * Recorded as a host request, not written to the pin. Arbitration still runs
 * afterwards, so asking for the reverse lamp without reverse gear - or for
 * anything at all during load shedding - is still refused downstream. */
bool handle_set_lamp(const Frame& request, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);

    const uint8_t id = request.payload[0];
    if (id >= static_cast<uint8_t>(LampId::Count) || c == 0 || c->host_request == 0) {
        response.payload[0] = static_cast<uint8_t>(Status::BadParameter);
        return true;
    }

    const bool on = (request.payload[1] != 0U);
    c->host_request->set(static_cast<LampId>(id), on ? LampDrive::On : LampDrive::Off);
    c->host_mask = static_cast<uint16_t>(c->host_mask | (1U << id));
    return true;
}

/* 0x05 GET_LAMP <- lampId -> actual state after arbitration. */
bool handle_get_lamp(const Frame& request, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);

    const uint8_t id = request.payload[0];
    if (id >= static_cast<uint8_t>(LampId::Count) || c == 0 || c->actual == 0) {
        response.payload[0] = static_cast<uint8_t>(Status::BadParameter);
        return true;
    }

    response.len        = 2U;
    response.payload[1] =
        (c->actual->get(static_cast<LampId>(id)) != LampDrive::Off) ? 1U : 0U;
    return true;
}

/* 0x08 GET_BATTERY -> per mille, big-endian. */
bool handle_get_battery(const Frame&, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);

    uint16_t permille = 0U;
    if (c != 0 && c->battery != 0) { permille = c->battery->permille(); }

    response.len        = 3U;
    response.payload[1] = static_cast<uint8_t>((permille >> 8) & 0xFFU);
    response.payload[2] = static_cast<uint8_t>(permille & 0xFFU);
    return true;
}

/* 0x09 GET_DTC -> count, then one byte per active code. */
bool handle_get_dtc(const Frame&, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);
    if (c == 0 || c->faults == 0) {
        response.len        = 2U;
        response.payload[1] = 0U;
        return true;
    }

    uint8_t codes[services::kMaxPayload];
    const uint8_t n = c->faults->active_codes(codes,
                          static_cast<uint8_t>(services::kMaxPayload - 2U));

    response.len        = static_cast<uint8_t>(2U + n);
    response.payload[1] = n;
    for (uint8_t i = 0U; i < n; ++i) { response.payload[2U + i] = codes[i]; }
    return true;
}

/* 0x0A CLEAR_DTC. */
bool handle_clear_dtc(const Frame&, Frame&, void* context)
{
    CommandContext* c = ctx_of(context);
    if (c != 0 && c->faults != 0) { c->faults->clear_all(); }
    return true;
}

/* 0x0B RESET - flagged rather than executed here, so the reply is never cut
 * off mid-transmission. The main loop performs the reset. */
bool handle_reset(const Frame&, Frame&, void* context)
{
    CommandContext* c = ctx_of(context);
    if (c != 0) { c->reset_requested = true; }
    return true;
}

/* 0x0C HEARTBEAT - CommMgr has already refreshed the link timer by the time
 * this runs; the status reply is the whole point. */
bool handle_heartbeat(const Frame&, Frame&, void*) { return true; }

/* 0x06 / 0x07 - raise a request; the application owns the Door FSM and its
 * open-door guard, so a lock here can still be refused (SRS-DOOR-002). */
bool handle_door_lock(const Frame&, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);
    if (c == 0) {
        response.payload[0] = static_cast<uint8_t>(Status::BadParameter);
        return true;
    }
    c->door_lock_requested = true;
    return true;
}

bool handle_door_unlock(const Frame&, Frame& response, void* context)
{
    CommandContext* c = ctx_of(context);
    if (c == 0) {
        response.payload[0] = static_cast<uint8_t>(Status::BadParameter);
        return true;
    }
    c->door_unlock_requested = true;
    return true;
}

const CommandEntry kTable[] = {
    { static_cast<uint8_t>(Cmd::Ping),       handle_ping,        0U },
    { static_cast<uint8_t>(Cmd::GetVersion), handle_get_version, 0U },
    { static_cast<uint8_t>(Cmd::GetStatus),  handle_get_status,  0U },
    { static_cast<uint8_t>(Cmd::SetLamp),    handle_set_lamp,    2U },
    { static_cast<uint8_t>(Cmd::GetLamp),    handle_get_lamp,    1U },
    { static_cast<uint8_t>(Cmd::DoorLock),   handle_door_lock,   0U },
    { static_cast<uint8_t>(Cmd::DoorUnlock), handle_door_unlock, 0U },
    { static_cast<uint8_t>(Cmd::GetBattery), handle_get_battery, 0U },
    { static_cast<uint8_t>(Cmd::GetDtc),     handle_get_dtc,     0U },
    { static_cast<uint8_t>(Cmd::ClearDtc),   handle_clear_dtc,   0U },
    { static_cast<uint8_t>(Cmd::Reset),      handle_reset,       0U },
    { static_cast<uint8_t>(Cmd::Heartbeat),  handle_heartbeat,   0U },
};

}  // namespace

const services::CommandEntry* command_table() { return kTable; }

uint8_t command_table_size()
{
    return static_cast<uint8_t>(sizeof(kTable) / sizeof(kTable[0]));
}

}  // namespace app
}  // namespace bcm
