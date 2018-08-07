// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_encoder.h
 */
#ifndef _UGCS_VSM_MAVLINK_ENCODER_H_
#define _UGCS_VSM_MAVLINK_ENCODER_H_

#include <ugcs/vsm/io_buffer.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Encoder capable of creating byte buffers based on Mavlink payload and
 * identifiers.
 */
class Mavlink_encoder {
public:
    /** Encode Mavlink version 1 message.
     * @param payload Payload.
     * @param system_id System id.
     * @param component_id Component id.
     * @return Byte buffer ready to be directly written on to the wire.
     */
    Io_buffer::Ptr
    Encode_v1(const mavlink::Payload_base& payload,
        uint8_t system_id, uint8_t component_id)
    {
        /* Reserve space for the whole packet. */
        std::vector<uint8_t> data(mavlink::MAVLINK_1_HEADER_LEN + payload.Get_size_v1() +
                sizeof(uint16_t));

        /* Fill the header. */
        data[0] = mavlink::START_SIGN;
        data[1] = payload.Get_size_v1();
        data[2] = seq++;
        data[3] = system_id;
        data[4] = component_id;
        ASSERT(payload.Get_id() < 256);
        data[5] = static_cast<uint8_t>(payload.Get_id());

        /* If this affects performance, then Payload class should be redesigned
         * to provide space for header and checksum in-place.
         */
        memcpy(&data[mavlink::MAVLINK_1_HEADER_LEN], payload.Get_buffer()->Get_data(),
               payload.Get_size_v1());

        /* Don't include start sign. */
        mavlink::Checksum sum(&data[1], mavlink::MAVLINK_1_HEADER_LEN + payload.Get_size_v1() - 1);
        uint16_t sum_val = sum.Accumulate(payload.Get_extra_byte());
        mavlink::Uint16 *wire_sum =
            reinterpret_cast<mavlink::Uint16*>(&data[mavlink::MAVLINK_1_HEADER_LEN + payload.Get_size_v1()]);
        *wire_sum = sum_val;

        return Io_buffer::Create(std::move(data));
    }
    /** Encode Mavlink version 2 message.
     * @param payload Payload.
     * @param system_id System id.
     * @param component_id Component id.
     * @return Byte buffer ready to be directly written on to the wire.
     */
    Io_buffer::Ptr
    Encode_v2(const mavlink::Payload_base& payload,
        uint8_t system_id, uint8_t component_id)
    {
        /* Reserve space for the whole packet. */
        std::vector<uint8_t> data(mavlink::MAVLINK_2_HEADER_LEN + payload.Get_size_v2() +
                sizeof(uint16_t));

        /* Fill the header. */
        data[0] = mavlink::START_SIGN2;
        data[2] = 0;    // incompat_flags
        data[3] = 0;    // compat_flags
        data[4] = seq++;
        data[5] = system_id;
        data[6] = component_id;
        data[7] = static_cast<uint8_t>(payload.Get_id() >> 0);
        data[8] = static_cast<uint8_t>(payload.Get_id() >> 8);
        data[9] = static_cast<uint8_t>(payload.Get_id() >> 16);

        memcpy(&data[mavlink::MAVLINK_2_HEADER_LEN], payload.Get_buffer()->Get_data(), payload.Get_size_v2());

        auto packet_len = payload.Get_size_v2();

        // trim trailing zeroes.
        for (; packet_len > 1 && data[mavlink::MAVLINK_2_HEADER_LEN + packet_len - 1] == 0; packet_len--) {
        }

        data[1] = packet_len;

        /* Don't include start sign and any extension fields*/
        mavlink::Checksum sum(&data[1], mavlink::MAVLINK_2_HEADER_LEN + packet_len - 1);

        uint16_t sum_val = sum.Accumulate(payload.Get_extra_byte());
        mavlink::Uint16 *wire_sum =
            reinterpret_cast<mavlink::Uint16*>(&data[mavlink::MAVLINK_2_HEADER_LEN + packet_len]);
        *wire_sum = sum_val;

        data.resize(mavlink::MAVLINK_2_HEADER_LEN + packet_len + 2);

        return Io_buffer::Create(std::move(data));
    }

private:
    /** Current sequence number. */
    uint8_t seq = 0;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_MAVLINK_ENCODER_H_ */
