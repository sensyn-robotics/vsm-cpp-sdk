// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_decoder.h
 *
 * Mavlink decoder
 */
#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/mavlink.h>

#ifndef _MAVLINK_DECODER_H_
#define _MAVLINK_DECODER_H_

namespace ugcs {
namespace vsm {

/** Decodes Mavlink 1.0 messages from byte stream. */
template<typename Mavlink_kind>
class Mavlink_decoder {

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

public:

    /** Mavlink header type. */
    typedef mavlink::Header<Mavlink_kind> Header;

    /** Handler type of the received Mavlink message. Arguments are:
     * - Payload buffer
     * - Message id
     * - Sending system id
     * - Sending component id
     */
    typedef Callback_proxy<void, Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE,
            typename Mavlink_kind::System_id, uint8_t, uint32_t> Handler;

    /** Handler for the raw data going through the decoder. */
    typedef Callback_proxy<void, Io_buffer::Ptr> Raw_data_handler;

    /** Convenience builder for Mavlink decoder handlers. */
    DEFINE_CALLBACK_BUILDER(
            Make_decoder_handler,
            (Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE, typename Mavlink_kind::System_id, uint8_t, uint32_t),
            (nullptr, mavlink::MESSAGE_ID::DEBUG_VALUE, mavlink::SYSTEM_ID_NONE, 0, 0))

    /** Convenience builder for raw data handlers. */
    DEFINE_CALLBACK_BUILDER(Make_raw_data_handler, (Io_buffer::Ptr), (nullptr))

    /** Decoder statistics. */
    struct Stats {
        /** Messages processed by registered handler. */
        uint64_t handled;
        /** Messages without handler, dropped. */
        uint64_t no_handler;
        /** Messages with wrong checksum. */
        uint64_t bad_checksum;
        /** Messages with wrong length, but correct checksum. */
        uint64_t bad_length;
        /** Messages with unknown id. Note that checksum is not verified for
         * such messages.
         */
        uint64_t unknown_id;
        /** Total received bytes, including any error or not handled messages. */
        uint64_t bytes_received;
        /** Number of STX bytes found during decoding, i.e. how many times packet
         * decode was tried to be started. */
        uint64_t stx_syncs;
    };


    /** Default constructor. */
    Mavlink_decoder():
        stats()
    {}

    /** Delete copy constructor. */
    Mavlink_decoder(const Mavlink_decoder&) = delete;

    /** Should be called prior to intention to delete the instance. */
    void
    Disable()
    {
        handler = Handler();
        data_handler = Raw_data_handler();
    }

    /**
     * Register handler for successfully decoded Mavlink messages.
     */
    void
    Register_handler(Handler handler)
    {
        this->handler = handler;
    }

    void
    Register_raw_data_handler(Raw_data_handler handler)
    {
        data_handler = handler;
    }

    /** Decode buffer from the wire. */
    void
    Decode(Io_buffer::Ptr buffer)
    {
        if (data_handler) {
            data_handler(buffer);
        }
        Decode_buffer(buffer);

        /* Do we have accumulated segments which could not be processed by a
         * full turn of state machine, i.e. back to STX?
         */
        while (segments.size() && state == State::STX) {
            auto retry_segs = std::move(segments);
            do {
                Decode_buffer(retry_segs.front());
                retry_segs.pop_front();
                /* Loop while in the middle of packet decoding. */
            } while (retry_segs.size() && state != State::STX);
            /* Join newly accumulated and remaining segments. */
            segments.splice(segments.end(), retry_segs);
        }
    }

    /** Reset decoder to initial state.
     * @param reset_stats @a true if statistics should also be reset.
     */
    void
    Reset(bool reset_stats = true)
    {
        state = State::STX;
        payload = nullptr;
        header.clear();
        checksum.clear();
        if (reset_stats) {
            stats = Stats();
        }
    }

    /** Get the exact number of bytes which should be read by underlying
     * I/O subsystem and fed to the decoder.
     * @return Exact number of bytes to be read by next read operation.
     */
    size_t
    Get_next_read_size() const
    {
        const Header* header_ptr;

        switch (state) {
        /*
         * In STX and HEADER states we don't want to read any payload data,
         * because we want to receive the whole payload at once together with
         * checksum to avoid memory copy.
         */
        case State::STX: return sizeof(Header);
        case State::HEADER: return sizeof(Header) - header.size();
        case State::PAYLOAD:
            header_ptr =
                    reinterpret_cast<const Header*>(header.data());
            return header_ptr->payload_len - payload->Get_length() + sizeof(uint16_t);
        case State::CHECKSUM: return sizeof(uint16_t) - checksum.size();
        default:
            ASSERT(false);
            VSM_EXCEPTION(Internal_error_exception, "Unexpected state %d",
                          static_cast<int>(state));
        }
    }

    /** Get read-only access to statistics. */
    const Mavlink_decoder::Stats &
    Get_stats() const
    {
        return stats;
    }

private:

    /** Decode single buffer and accumulate segments. */
    void
    Decode_buffer(Io_buffer::Ptr buffer)
    {
        stats.bytes_received += buffer->Get_length();

        while (buffer->Get_length()) {
            switch (state) {
            case State::STX:
                buffer = Decode_stx(buffer);
                break;
            case State::HEADER:
                buffer = Decode_header(buffer);
                break;
            case State::PAYLOAD:
                buffer = Decode_payload(buffer);
                break;
            case State::CHECKSUM:
                buffer = Decode_checksum(buffer);
                break;
            }
        }
    }

    /** Decode start sign. */
    Io_buffer::Ptr
    Decode_stx(Io_buffer::Ptr buffer)
    {
        const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
        size_t full_len = buffer->Get_length();
        size_t len = full_len;

        for ( ; len ; len--, data++) {
            if (*data == mavlink::START_SIGN) {
                stats.stx_syncs++;
                state = State::HEADER;
                header.clear();
                break;
            }
        }

        return buffer->Slice(full_len - len);
    }

    /** Decoder header. */
    Io_buffer::Ptr
    Decode_header(Io_buffer::Ptr buffer)
    {
        const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
        size_t len = buffer->Get_length();

        for ( ; len && header.size() < sizeof(Header) ; len--, data++) {
            header.push_back(*data);
        }

        if (header.size() == sizeof(Header)) {
            state = State::PAYLOAD;
            /* Strip STX byte. */
            segments.push_back(Io_buffer::Create(&header[1], header.size() - 1));
            payload = Io_buffer::Create();
        }

        return buffer->Slice(buffer->Get_length() - len);
    }

    /** Decoder payload. */
    Io_buffer::Ptr
    Decode_payload(Io_buffer::Ptr buffer)
    {
        Header* header_ptr = reinterpret_cast<Header*>(header.data());

        size_t to_read = std::min(header_ptr->payload_len - payload->Get_length(),
                                  buffer->Get_length());

        payload = payload->Concatenate(buffer->Slice(0, to_read));

        if (payload->Get_length() == header_ptr->payload_len) {
            state = State::CHECKSUM;
            segments.push_back(payload);
            checksum.clear();
        }

        return buffer->Slice(to_read);
    }

    /** Decoder checksum. */
    Io_buffer::Ptr
    Decode_checksum(Io_buffer::Ptr buffer)
    {
        const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
        size_t len = buffer->Get_length();

        for ( ; len && checksum.size() < sizeof(uint16_t) ; len--, data++) {
            checksum.push_back(*data);
        }

        if (checksum.size() == sizeof(uint16_t)) {
            /* Got checksum, state machine should be restarted. */
            state = State::STX;
            segments.push_back(Io_buffer::Create(&checksum[0], checksum.size()));
            Header* header_ptr = reinterpret_cast<Header*>(header.data());

            mavlink::Checksum sum(&header[1], header.size() - 1);
            sum.Accumulate(payload);

            mavlink::Extra_byte_length_pair crc_byte_len_pair;
            // Try all extensions.
            if (    !sum.Get_extra_byte_length_pair(header_ptr->message_id, crc_byte_len_pair, mavlink::Extension::Get())
                &&  !sum.Get_extra_byte_length_pair(header_ptr->message_id, crc_byte_len_pair, mavlink::apm::Extension::Get())
                &&  !sum.Get_extra_byte_length_pair(header_ptr->message_id, crc_byte_len_pair, mavlink::sph::Extension::Get())) {
                stats.unknown_id++;
                LOG_DEBUG("Unknown Mavlink message id: %d", header_ptr->message_id.Get());
                /* Save remaining bytes. */
                segments.push_back(buffer->Slice(buffer->Get_length() - len));
                /* And terminate the decoding of current buffer. */
                return Io_buffer::Create();
            }

            if (header_ptr->message_id.Get() == mavlink::MESSAGE_ID::GPS_RTCM_DATA) {
                // GPS_RTCM_DATA is not supported by SDK.
                /* Save remaining bytes. */
                segments.push_back(buffer->Slice(buffer->Get_length() - len));
                /* And terminate the decoding of current buffer. */
                return Io_buffer::Create();
            }

            uint16_t sum_calc = sum.Accumulate(crc_byte_len_pair.first);
            /* Convert checksum in Mavlink byte order to a host byte order
             * compatible type. */
            mavlink::Uint16* sum_recv = reinterpret_cast<mavlink::Uint16*>(checksum.data());

            bool cksum_ok = sum_calc == *sum_recv;
            bool length_ok = crc_byte_len_pair.second == header_ptr->payload_len;

            if (cksum_ok && length_ok) {
                /*
                 * Fully valid packet received, not interested in its segments
                 * anymore.
                 */
                segments.clear();
                mavlink::MESSAGE_ID_TYPE message_id = header_ptr->message_id.Get();
                if (handler) {
                    handler(payload, message_id, header_ptr->system_id, header_ptr->component_id, header_ptr->seq);
                    stats.handled++;
                } else {
                    stats.no_handler++;
                    LOG_DEBUG("Mavlink message %d handler not registered.", message_id);
                }
            } else {
                if (!cksum_ok) {
                    stats.bad_checksum++;
                } else {
                    stats.bad_length++;
                    LOG_DEBUG("Mavlink payload length mismatch, recv=%d wanted=%d.",
                            header_ptr->payload_len.Get(), crc_byte_len_pair.second);
                }
                /* Save remaining bytes. */
                segments.push_back(buffer->Slice(buffer->Get_length() - len));
                /* And terminate the decoding of current buffer. */
                return Io_buffer::Create();
            }
        }

        return buffer->Slice(buffer->Get_length() - len);
    }


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

    /** Raw data handler. */
    Raw_data_handler data_handler;

    /** Statistics. */
    Stats stats;

    /** Segments of the packet being decoded ordered by receiving time. First
     * segment does not include the STX byte in order to be able to continue
     * packet decoding just by skipping the first byte in a hope to find some
     * inner packet in case of STX synchronization is lost. */
    std::list<Io_buffer::Ptr> segments;

};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _MAVLINK_DECODER_H_ */
