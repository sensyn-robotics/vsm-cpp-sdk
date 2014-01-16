// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_decoder.h
 *
 * Mavlink decoder
 */
#include <vsm/callback.h>
#include <vsm/mavlink.h>

#ifndef _MAVLINK_DECODER_H_
#define _MAVLINK_DECODER_H_

namespace vsm {

/** Decodes Mavlink 1.0 messages from byte stream. */
class Mavlink_decoder {

public:

    /** Handler type of the received Mavlink message.
     * XXX describe callback arguments
     */
    typedef Callback_proxy<void, Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE,
                           mavlink::System_id, uint8_t> Handler;

    /** Decoder statistics. */
    struct Stats {
        /** Messages processed by registered handler. */
        uint64_t handled;
        /** Messages without handler, dropped. */
        uint64_t no_handler;
        /** Messages with wrong checksum. */
        uint64_t bad_checksum;
        /** Messages with unknown id. Note that checksum is not verified for
         * such messages.
         */
        uint64_t unknown_id;
        /** Total received bytes, including any error or not handled messages. */
        uint64_t bytes_received;
    };


    /** Default constructor. */
    Mavlink_decoder(const mavlink::Extension& extension = mavlink::Extension::Get()):
        stats(),
        extension(extension)
    {}

    /** Delete copy constructor. */
    Mavlink_decoder(const Mavlink_decoder&) = delete;

    /** Should be called prior to intention to delete the instance. */
    void
    Disable();

    /**
     * Register handler for successfully decoded Mavlink messages.
     */
    void
    Register_handler(Handler handler)
    {
        this->handler = handler;
    }

    /** Decode buffer from the wire. */
    void
    Decode(Io_buffer::Ptr buffer);

    /** Reset decoder to initial state.
     * @param reset_stats @a true if statistics should also be reset.
     */
    void
    Reset(bool reset_stats = true);

    /** Get the exact number of bytes which should be read by underlying
     * I/O subsystem and fed to the decoder.
     * @return Exact number of bytes to be read by next read operation.
     */
    size_t
    Get_next_read_size() const;

    /** Get read-only access to statistics. */
    const Stats &
    Get_stats() const;

private:

    /** Decoding state. */
    enum class State {
        /** Waiting for start sign. */
        STX,
        /** Receiving fixed header. */
        HEADER,
        /** Receiving payload. */
        PAYLOAD,
        /** Receiving checksum. */
        CHECKSUM
    };

    /** Decode start sign. */
    Io_buffer::Ptr
    Decode_stx(Io_buffer::Ptr buffer);

    /** Decoder header. */
    Io_buffer::Ptr
    Decode_header(Io_buffer::Ptr buffer);

    /** Decoder payload. */
    Io_buffer::Ptr
    Decode_payload(Io_buffer::Ptr buffer);

    /** Decoder checksum. */
    Io_buffer::Ptr
    Decode_checksum(Io_buffer::Ptr buffer);

    /** Current decoder state. */
    State state = State::STX;

    /** Header of the currently receiving message. */
    std::vector<uint8_t> header;

    /** Checksum of the currently receiving message. */
    std::vector<uint8_t> checksum;

    /** Payload of the currently receiving message. */
    Io_buffer::Ptr payload;

    /** Handler for decoded messages. */
    Handler handler;

    /** Statistics. */
    Stats stats;

    /** Mavlink extension. */
    const mavlink::Extension& extension;
};

/** Convenience builder for Mavlink decoder handlers. */
DEFINE_CALLBACK_BUILDER(
        Make_mavlink_decoder_handler,
        (Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE, mavlink::System_id, uint8_t),
        (nullptr, mavlink::MESSAGE_ID::DEBUG_VALUE, mavlink::SYSTEM_ID_NONE, 0))

} /* namespace vsm */

#endif /* _MAVLINK_DECODER_H_ */
