/**
 * @file    frame_codec.cpp
 * @brief   Frame encoding and byte-stream parsing.
 */
#include "frame_codec.h"

namespace bcm {
namespace services {

bool FrameCodec::encode(const Frame& frame,
                        uint8_t*     out,
                        uint16_t     out_size,
                        uint16_t&    written)
{
    written = 0U;

    if (out == 0)                     { return false; }
    if (frame.len > kMaxPayload)      { return false; }

    const uint16_t need = encoded_size(frame);
    if (out_size < need)              { return false; }

    Crc8     crc;
    uint16_t i = 0U;

    out[i++] = kFrameHeader;

    out[i++] = frame.cmd;
    crc.update(frame.cmd);

    out[i++] = frame.len;
    crc.update(frame.len);

    for (uint8_t p = 0U; p < frame.len; ++p) {
        out[i++] = frame.payload[p];
        crc.update(frame.payload[p]);
    }

    out[i++] = crc.value();
    out[i++] = kFrameFooter;

    written = i;
    return true;
}

void FrameParser::reset()
{
    state_ = State::Header;
    index_ = 0U;
    crc_.reset();
}

FrameParser::Result FrameParser::fail(Result why)
{
    ++frames_bad_;
    reset();
    return why;
}

FrameParser::Result FrameParser::feed(uint8_t byte)
{
    switch (state_) {

    case State::Header:
        /* Silently discard anything that is not a header - this is how the
         * parser recovers from a corrupted frame or a mid-stream connect. */
        if (byte == kFrameHeader) {
            crc_.reset();
            frame_.cmd = 0U;
            frame_.len = 0U;
            index_     = 0U;
            state_     = State::Command;
        }
        return Result::NeedMore;

    case State::Command:
        frame_.cmd = byte;
        crc_.update(byte);
        state_ = State::Length;
        return Result::NeedMore;

    case State::Length:
        if (byte > kMaxPayload) {
            /* A bogus length would otherwise make us swallow arbitrary
             * bytes, so reject immediately and resynchronise. */
            return fail(Result::Overflow);
        }
        frame_.len = byte;
        crc_.update(byte);
        index_ = 0U;
        state_ = (byte == 0U) ? State::Checksum : State::Payload;
        return Result::NeedMore;

    case State::Payload:
        frame_.payload[index_] = byte;
        crc_.update(byte);
        ++index_;
        if (index_ >= frame_.len) { state_ = State::Checksum; }
        return Result::NeedMore;

    case State::Checksum:
        if (byte != crc_.value()) { return fail(Result::ChecksumError); }
        state_ = State::Footer;
        return Result::NeedMore;

    case State::Footer:
        if (byte != kFrameFooter) { return fail(Result::FramingError); }
        ++frames_ok_;
        state_ = State::Header;
        index_ = 0U;
        return Result::Complete;

    default:
        return fail(Result::FramingError);
    }
}

}  // namespace services
}  // namespace bcm
