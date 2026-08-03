/**
 * @file    dispatcher.h
 * @brief   Command -> handler routing - pure logic, no HAL.
 *
 * A table, not a switch-ladder: adding a command is one table entry plus a
 * handler, with no existing code edited (Open/Closed - architecture document
 * section 3.1). The dispatcher knows nothing about framing.
 */
#ifndef BCM_DISPATCHER_H
#define BCM_DISPATCHER_H

#include "protocol.h"

namespace bcm {
namespace services {

/**
 * @brief Handler signature.
 * @param request  decoded request frame.
 * @param response pre-filled with cmd = request|kResponseFlag and
 *                 payload[0] = Status::Ok; the handler adjusts as needed.
 * @param context  opaque pointer supplied at construction.
 * @return false to indicate the handler produced no response (fire-and-forget).
 */
typedef bool (*CommandHandler)(const Frame& request,
                               Frame&       response,
                               void*        context);

struct CommandEntry {
    uint8_t        cmd;
    CommandHandler handler;
    uint8_t        min_payload;   ///< shortest acceptable request payload
};

class Dispatcher {
public:
    Dispatcher(const CommandEntry* table, uint8_t count, void* context = 0)
        : table_(table), count_(count), context_(context),
          handled_(0U), rejected_(0U)
    {
    }

    /**
     * @brief Route one request.
     * @return true if @p response should be sent.
     *
     * Unknown commands and short payloads are answered with an error status
     * rather than dropped - silence is indistinguishable from a dead link,
     * and the host needs to tell those apart.
     */
    bool dispatch(const Frame& request, Frame& response);

    uint32_t handled() const { return handled_; }
    uint32_t rejected() const { return rejected_; }

private:
    const CommandEntry* table_;
    uint8_t             count_;
    void*               context_;
    uint32_t            handled_;
    uint32_t            rejected_;

    const CommandEntry* find(uint8_t cmd) const;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_DISPATCHER_H */
