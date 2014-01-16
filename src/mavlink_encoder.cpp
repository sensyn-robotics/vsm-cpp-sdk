// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/mavlink_encoder.h>

using namespace vsm;

Io_buffer::Ptr
Mavlink_encoder::Encode(const mavlink::Payload_base& payload,
                        mavlink::System_id system_id, uint8_t component_id)
{
    /* Reserve space for the whole packet. */
    std::vector<uint8_t> data(sizeof(mavlink::Header) + payload.Get_size() +
                              sizeof(uint16_t));

    mavlink::Header* hdr = reinterpret_cast<mavlink::Header*>(data.data());

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
    memcpy(&data[sizeof(mavlink::Header)], payload.Get_buffer()->Get_data(),
           payload.Get_size());

    /* Don't include start sign. */
    mavlink::Checksum sum(&data[1], sizeof(mavlink::Header) + payload.Get_size() - 1);
    uint16_t sum_val = sum.Accumulate(payload.Get_extra_byte());
    mavlink::Uint16 *wire_sum =
        reinterpret_cast<mavlink::Uint16*>(&data[sizeof(mavlink::Header) + payload.Get_size()]);
    *wire_sum = sum_val;

    return Io_buffer::Create(std::move(data));
}
