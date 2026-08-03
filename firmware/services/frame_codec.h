/**
 * @file    frame_codec.h
 * @brief   Frame encoding and byte-stream parsing - pure logic, no HAL.
 *
 * Deliberately knows nothing about what any command *means*; that belongs to
 * the dispatcher. Splitting the two is what lets the wire format be verified
 * exhaustively on the host and shared byte-for-byte with the C# tool
 * (architecture document, section 6).
 */
#ifndef BCM_FRAME_CODEC_H
#define BCM_FRAME_CODEC_H

#include "protocol.h"
#include "crc8.h"

namespace bcm {
namespace services {

/**
 * @brief Serialises a Frame onto the wire.
 */
class FrameCodec {
public:
    /**
     * @brief Encode a frame into a caller-supplied buffer.
     * @param frame     source frame; len must be <= kMaxPayload.
     * @param out       destination buffer.
     * @param out_size  capacity of @p out.
     * @param written   receives the number of bytes produced.
     * @return false if the payload is oversized or the buffer too small.
     *         @p out is left untouched on failure.
     */
    static bool encode(const Frame& frame,
                       uint8_t*     out,
                       uint16_t     out_size,
                       uint16_t&    written);

    /// Bytes an encoded frame will occupy.
    static uint16_t encoded_size(const Frame& frame)
    {
        return static_cast<uint16_t>(frame.len + kFrameOverhead);
    }
};

/**
 * @brief Incremental byte-at-a-time frame parser.
 *
 * Fed one received byte at a time so it can run straight out of the UART
 * ring buffer with no intermediate copy. On a bad checksum or missing footer
 * it reports the error and resynchronises by hunting for the next header,
 * so a corrupted frame costs one frame rather than desynchronising the link
 * permanently.
 */
class FrameParser {
public:
    enum class Result : uint8_t {
        NeedMore,        ///< byte consumed, frame not finished
        Complete,        ///< frame() is now valid
        ChecksumError,   ///< CRC mismatch; parser resynchronised
        FramingError,    ///< footer missing; parser resynchronised
        Overflow         ///< LEN exceeded kMaxPayload; frame rejected
    };

    /* Counters live outside reset() - they must survive a resynchronisation,
     * otherwise the error statistics would clear themselves on every fault. */
    FrameParser()
        : state_(State::Header), frame_(), index_(0U), crc_(),
          frames_ok_(0U), frames_bad_(0U)
    {
        reset();
    }

    /// Feed one received byte.
    Result feed(uint8_t byte);

    /// Valid only immediately after feed() returned Complete.
    const Frame& frame() const { return frame_; }

    /// Abandon any partial frame and hunt for the next header.
    void reset();

    /// Frames successfully decoded since construction.
    uint32_t frames_ok() const { return frames_ok_; }

    /// Frames rejected for any reason since construction.
    uint32_t frames_bad() const { return frames_bad_; }

private:
    enum class State : uint8_t { Header, Command, Length, Payload, Checksum, Footer };

    State    state_;
    Frame    frame_;
    uint8_t  index_;
    Crc8     crc_;
    uint32_t frames_ok_;
    uint32_t frames_bad_;

    Result fail(Result why);
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_FRAME_CODEC_H */
