// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_encoder.h
 */
#ifndef _MAVLINK_ENCODER_H_
#define _MAVLINK_ENCODER_H_

#include <ugcs/vsm/io_buffer.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Encoder capable of creating byte buffers based on Mavlink payload and
 * identifiers.
 */
class Mavlink_encoder {
public:
    /** Encode Mavlink message.
     * @param payload Payload.
     * @param system_id System id.
     * @param component_id Component id.
     * @return Byte buffer ready to be directly written on to the wire.
     */
    template<typename Mavlink_kind>
    Io_buffer::Ptr
    Encode(const mavlink::Payload_base& payload,
            typename Mavlink_kind::System_id system_id, uint8_t component_id)
    {
        using Header = mavlink::Header<Mavlink_kind>;

        /* Reserve space for the whole packet. */
        std::vector<uint8_t> data(sizeof(Header) + payload.Get_size() +
                sizeof(uint16_t));

        Header* hdr = reinterpret_cast<Header*>(data.data());

        /* Fill the header. */
        hdr->start_sign = mavlink::START_SIGN;
        hdr->payload_len = payload.Get_size();
        hdr->seq = seq++;
        hdr->system_id = system_id;
        hdr->component_id = component_id;
        hdr->message_id = static_cast<uint8_t>(payload.Get_id());

        /* If this affects performance, then Payload class should be redesigned
         * to provide space for header and checksum in-place.
         */
        memcpy(&data[sizeof(Header)], payload.Get_buffer()->Get_data(),
               payload.Get_size());

        /* Don't include start sign. */
        mavlink::Checksum sum(&data[1], sizeof(Header) + payload.Get_size() - 1);
        uint16_t sum_val = sum.Accumulate(payload.Get_extra_byte());
        mavlink::Uint16 *wire_sum =
            reinterpret_cast<mavlink::Uint16*>(&data[sizeof(Header) + payload.Get_size()]);
        *wire_sum = sum_val;

        return Io_buffer::Create(std::move(data));
    }

    /** Encode Mavlink message.
     * @param payload Payload.
     * @param system_id System id.
     * @param component_id Component id.
     * @param request_id Request id.
     * @return Byte buffer ready to be directly written on to the wire.
     */
    template<typename Mavlink_kind>
    Io_buffer::Ptr
    Encode(
        const mavlink::Payload_base& payload,
        typename Mavlink_kind::System_id system_id,
        uint8_t component_id,
        uint32_t request_id)
    {
        using Header = mavlink::Header<Mavlink_kind>;

        /* Reserve space for the whole packet. */
        std::vector<uint8_t> data(sizeof(Header) + payload.Get_size() +
                sizeof(uint16_t));

        Header* hdr = reinterpret_cast<Header*>(data.data());

        /* Fill the header. */
        hdr->start_sign = mavlink::START_SIGN;
        hdr->payload_len = payload.Get_size();
        hdr->seq = request_id;
        hdr->system_id = system_id;
        hdr->component_id = component_id;
        hdr->message_id = static_cast<uint8_t>(payload.Get_id());

        /* If this affects performance, then Payload class should be redesigned
         * to provide space for header and checksum in-place.
         */
        memcpy(&data[sizeof(Header)], payload.Get_buffer()->Get_data(),
               payload.Get_size());

        /* Don't include start sign. */
        mavlink::Checksum sum(&data[1], sizeof(Header) + payload.Get_size() - 1);
        uint16_t sum_val = sum.Accumulate(payload.Get_extra_byte());
        mavlink::Uint16 *wire_sum =
            reinterpret_cast<mavlink::Uint16*>(&data[sizeof(Header) + payload.Get_size()]);
        *wire_sum = sum_val;

        return Io_buffer::Create(std::move(data));
    }
private:
    /** Current sequence number. */
    uint8_t seq = 0;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _MAVLINK_ENCODER_H_ */
