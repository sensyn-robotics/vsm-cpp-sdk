// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <ugcs/vsm/mavlink_decoder.h>
#include <ugcs/vsm/mavlink_encoder.h>

using namespace ugcs::vsm;


int received;
uint8_t sys_id;
uint8_t comp_id;
mavlink::MESSAGE_ID_TYPE msg_id;

void Mavlink_message_handler(Io_buffer::Ptr buffer,
        mavlink::MESSAGE_ID_TYPE message_id, uint8_t system_id,
        uint8_t component_id,
        uint8_t)
{
    mavlink::Pld_heartbeat hb(buffer);
    sys_id = system_id;
    comp_id = component_id;
    msg_id = message_id;
    received++;
}

Mavlink_encoder enc;

template<class Payload>
Io_buffer::Ptr
Build_message(Payload& pld)
{
    return enc.Encode_v2(pld, 1, 2);
}

TEST(mavlink_decoder_basic_tests)
{
    Mavlink_decoder decoder;

    mavlink::Uint16 mavlink_cksum = 0;
    mavlink::Pld_heartbeat hb;
    Io_buffer::Ptr message = Build_message(hb);

    decoder.Decode(message);
    decoder.Decode(message);

    decoder.Register_handler(
            Mavlink_decoder::Make_decoder_handler(
                    &Mavlink_message_handler));

    Io_buffer::Ptr msg_tmp = message;
    while (msg_tmp->Get_length()) {
        size_t size = decoder.Get_next_read_size();
        decoder.Decode(msg_tmp->Slice(0, size));
        msg_tmp = msg_tmp->Slice(size);
    }

    CHECK_EQUAL(2ul, decoder.Get_stats().no_handler);
    CHECK_EQUAL(1ul, decoder.Get_stats().handled);
    CHECK_EQUAL(0ul, decoder.Get_stats().bad_checksum);
    CHECK_EQUAL(1, received);
    CHECK_EQUAL(1, sys_id);
    CHECK_EQUAL(2, comp_id);
    CHECK(mavlink::MESSAGE_ID::HEARTBEAT == msg_id);

    decoder.Reset();
    decoder.Decode(message);
    CHECK_EQUAL(1ul, decoder.Get_stats().handled);
    CHECK_EQUAL(2, received);

    /* Spoil the checksum .*/
    message = message->Slice(0, message->Get_length() - sizeof(uint16_t));
    message = message->Concatenate(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));
    decoder.Decode(message);
    CHECK_EQUAL(1ul, decoder.Get_stats().handled);
    CHECK_EQUAL(1ul, decoder.Get_stats().bad_checksum);
    CHECK_EQUAL(2, received);

    /* There is no 0xff message id yet. */
    void * d = const_cast<void*>(message->Get_data());
    *(static_cast<uint8_t*>(d) + 7) = 0xff;
    decoder.Decode(message);
    CHECK_EQUAL(1ul, decoder.Get_stats().unknown_id);
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
                        mavlink::MESSAGE_ID::NAMED_VALUE_FLOAT,
                        mavlink::MESSAGE_ID::HIL_STATE,
                        static_cast<mavlink::MESSAGE_ID>(3),
                        static_cast<mavlink::MESSAGE_ID>(8),
                        static_cast<mavlink::MESSAGE_ID>(9)
                      }) {
        Mavlink_decoder decoder;
        mavlink::Pld_heartbeat hb;
        mavlink::Pld_change_operator_control_ack chg_ctrl;
        Io_buffer::Ptr hb_message = Build_message(hb);
        Io_buffer::Ptr chg_ctrl_message = Build_message(chg_ctrl);
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
        CHECK_EQUAL(4ul, decoder.Get_stats().no_handler);
        CHECK_EQUAL((sizeof(padding) / 2) * 5 + 4, decoder.Get_stats().stx_syncs);
        mavlink::Extra_byte_length_pair pair;
        if (mavlink::Checksum::Get_extra_byte_length_pair(filler, pair)) {
            CHECK(decoder.Get_stats().bad_checksum > 100);
        } else {
            CHECK(decoder.Get_stats().unknown_id > 100);
        }
    }
}

