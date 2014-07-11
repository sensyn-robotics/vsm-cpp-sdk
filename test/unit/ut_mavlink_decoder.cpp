// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <ugcs/vsm/mavlink_decoder.h>

using namespace ugcs::vsm;

typedef mavlink::Mavlink_kind_standard Mavlink_kind;

int received;
Mavlink_kind::System_id sys_id;
uint8_t comp_id;
mavlink::MESSAGE_ID_TYPE msg_id;

void Mavlink_message_handler(Io_buffer::Ptr buffer,
        mavlink::MESSAGE_ID_TYPE message_id, Mavlink_kind::System_id system_id,
        uint8_t component_id)
{
    mavlink::Pld_heartbeat hb(buffer);
    sys_id = system_id;
    comp_id = component_id;
    msg_id = message_id;
    received++;
}

template<class Payload>
Io_buffer::Ptr
Build_message(mavlink::Uint16& checksum, mavlink::Header<Mavlink_kind>& header,
        Payload& hb)
{
    header.start_sign = mavlink::START_SIGN;
    header.payload_len = hb.Get_size();
    header.seq = 0;
    header.system_id = 1;
    header.component_id = 2;
    header.message_id = static_cast<uint8_t>(hb.Get_id());
    auto message = Io_buffer::Create(&header, sizeof(header));

    message = message->Concatenate(hb.Get_buffer());

    mavlink::Checksum sum((reinterpret_cast<uint8_t*>(&header)) + 1, sizeof(header) - 1);
    sum.Accumulate(hb.Get_buffer());
    uint16_t sum_val = sum.Accumulate(hb.Get_extra_byte());
    checksum = sum_val;
    return message->Concatenate(Io_buffer::Create(&sum_val, sizeof(sum_val)));
}

TEST(mavlink_decoder_basic_tests)
{
    Mavlink_decoder<Mavlink_kind> decoder;

    mavlink::Uint16 mavlink_cksum;
    mavlink::Header<Mavlink_kind> header;
    mavlink::Pld_heartbeat hb;
    Io_buffer::Ptr message = Build_message(mavlink_cksum, header, hb);

    decoder.Decode(Io_buffer::Create(&header, sizeof(header)));
    decoder.Decode(hb.Get_buffer());
    decoder.Decode(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));

    decoder.Decode(message);

    decoder.Register_handler(
            Mavlink_decoder<Mavlink_kind>::Make_decoder_handler(
                    &Mavlink_message_handler));

    Io_buffer::Ptr msg_tmp = message;
    while (msg_tmp->Get_length()) {
        size_t size = decoder.Get_next_read_size();
        decoder.Decode(msg_tmp->Slice(0, size));
        msg_tmp = msg_tmp->Slice(size);
    }

    CHECK_EQUAL(2, decoder.Get_stats().no_handler);
    CHECK_EQUAL(1, decoder.Get_stats().handled);
    CHECK_EQUAL(0, decoder.Get_stats().bad_checksum);
    CHECK_EQUAL(1, received);
    CHECK_EQUAL(1, sys_id);
    CHECK_EQUAL(2, comp_id);
    CHECK(mavlink::MESSAGE_ID::HEARTBEAT == msg_id);

    decoder.Reset();
    decoder.Decode(message);
    CHECK_EQUAL(1, decoder.Get_stats().handled);
    CHECK_EQUAL(2, received);

    /* Spoil the checksum .*/
    message = message->Slice(0, message->Get_length() - sizeof(uint16_t));
    mavlink_cksum = mavlink_cksum + 1;
    message = message->Concatenate(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));
    decoder.Decode(message);
    CHECK_EQUAL(1, decoder.Get_stats().handled);
    CHECK_EQUAL(1, decoder.Get_stats().bad_checksum);
    CHECK_EQUAL(2, received);

    /* There is no 0xff message id yet. */
    header.message_id = 0xff;
    message = Io_buffer::Create(&header, sizeof(header));
    message = message->Concatenate(hb.Get_buffer());
    mavlink::Checksum sum(reinterpret_cast<uint8_t*>(&header) + 1, sizeof(header) - 1);
    sum.Accumulate(hb.Get_buffer());
    uint16_t sum_val = sum.Accumulate(hb.Get_extra_byte());
    mavlink_cksum = sum_val;
    message = message->Concatenate(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));

    decoder.Decode(message);
    CHECK_EQUAL(1, decoder.Get_stats().unknown_id);
}

/* Test for false start signs. Decoder should be able to dig out exactly
 * two heartbeat and two change operator control ack messages from the byte
 * stream full of start signs followed by valid and invalid message identifiers.
 */
TEST(mavlink_decoder_wrong_stx)
{
    /* Filler values after STX bytes, valid and invalid. */
    for (auto filler: {
                        mavlink::MESSAGE_ID::PARAM_REQUEST_READ,
                        mavlink::MESSAGE_ID::SET_MODE,
                        mavlink::MESSAGE_ID::HEARTBEAT,
                        mavlink::MESSAGE_ID::ATTITUDE,
                        mavlink::MESSAGE_ID::GPS_STATUS,
                        mavlink::MESSAGE_ID::STATUSTEXT,
                        mavlink::MESSAGE_ID::HIL_STATE,
                        static_cast<mavlink::MESSAGE_ID>(3),
                        static_cast<mavlink::MESSAGE_ID>(8),
                        static_cast<mavlink::MESSAGE_ID>(9)
                      }) {
        Mavlink_decoder<Mavlink_kind> decoder;
        mavlink::Uint16 mavlink_cksum;
        mavlink::Header<Mavlink_kind> header;
        mavlink::Pld_heartbeat hb;
        mavlink::Pld_change_operator_control_ack chg_ctrl;
        Io_buffer::Ptr hb_message = Build_message(mavlink_cksum, header, hb);
        Io_buffer::Ptr chg_ctrl_message = Build_message(mavlink_cksum, header, chg_ctrl);
        uint8_t padding[512];
        std::memset(padding, mavlink::START_SIGN, sizeof(padding));
        /* Simulate start of the packet. */
        for (unsigned i = 0; i < sizeof(padding); i += 2) {
            padding[i] = filler;
        }
        uint8_t dummy_padding[512];
        std::memset(dummy_padding, 1, sizeof(dummy_padding));

        Io_buffer::Ptr buf = Io_buffer::Create(&padding[0], sizeof(padding));
        buf = buf->Concatenate(hb_message);
        buf = buf->Concatenate(Io_buffer::Create(padding, sizeof(padding)));
        buf = buf->Concatenate(hb_message);
        buf = buf->Concatenate(Io_buffer::Create(padding, sizeof(padding)));
        buf = buf->Concatenate(chg_ctrl_message);
        buf = buf->Concatenate(Io_buffer::Create(padding, sizeof(padding)));
        buf = buf->Concatenate(chg_ctrl_message);
        buf = buf->Concatenate(Io_buffer::Create(padding, sizeof(padding)));
        buf = buf->Concatenate(Io_buffer::Create(dummy_padding, sizeof(dummy_padding)));
        decoder.Decode(buf);
        /* 2 + 2 messages in total should be dig out. */
        CHECK_EQUAL(4, decoder.Get_stats().no_handler);
        CHECK_EQUAL((sizeof(padding) / 2) * 5 + 4, decoder.Get_stats().stx_syncs);
        try {
            /* Check for message id validness. */
            mavlink::Checksum sum;
            sum.Get_extra_byte_length_pair(filler);
            CHECK(decoder.Get_stats().bad_checksum > 100);
        } catch (...) {
            CHECK(decoder.Get_stats().unknown_id > 100);
        }
    }
}

