/**
 * @file    dispatcher.cpp
 * @brief   Command -> handler routing.
 */
#include "dispatcher.h"

namespace bcm {
namespace services {

const CommandEntry* Dispatcher::find(uint8_t cmd) const
{
    if (table_ == 0) { return 0; }

    for (uint8_t i = 0U; i < count_; ++i) {
        if (table_[i].cmd == cmd) { return &table_[i]; }
    }
    return 0;
}

bool Dispatcher::dispatch(const Frame& request, Frame& response)
{
    /* A response frame must never be mistaken for a request - otherwise two
     * BCMs on one bus, or a looped-back TX line, would answer each other
     * forever. */
    if ((request.cmd & kResponseFlag) != 0U) {
        ++rejected_;
        return false;
    }

    const CommandEntry* entry = find(request.cmd);

    if (entry == 0 || entry->handler == 0) {
        ++rejected_;
        response = make_status_response(request.cmd, Status::UnknownCmd);
        return true;
    }

    if (request.len < entry->min_payload) {
        ++rejected_;
        response = make_status_response(request.cmd, Status::BadLength);
        return true;
    }

    /* Pre-fill the success case so handlers only write what differs. */
    response = make_status_response(request.cmd, Status::Ok);

    const bool reply = entry->handler(request, response, context_);
    ++handled_;
    return reply;
}

}  // namespace services
}  // namespace bcm
