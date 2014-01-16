// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <vsm/mavlink_decoder.h>

using namespace vsm;

int received;
uint8_t sys_id;
uint8_t comp_id;
mavlink::MESSAGE_ID_TYPE msg_id;

void Mavlink_message_handler(Io_buffer::Ptr buffer,
        mavlink::MESSAGE_ID_TYPE message_id, mavlink::System_id system_id,
        uint8_t component_id)
{
    mavlink::Pld_heartbeat hb(buffer);
    sys_id = system_id;
    comp_id = component_id;
    msg_id = message_id;
    received++;
}

TEST(mavlink_decoder_basic_tests)
{
    Mavlink_decoder decoder;

    mavlink::Pld_heartbeat hb;
    Io_buffer::Ptr message;

    mavlink::Header header;
    header.start_sign = mavlink::START_SIGN;
    header.payload_len = hb.Get_size();
    header.seq = 0;
    header.system_id = 1;
    header.component_id = 2;
    header.message_id = static_cast<uint8_t>(hb.Get_id());
    message = Io_buffer::Create(&header, sizeof(header));

    message = message->Concatenate(hb.Get_buffer());

    mavlink::Checksum sum((reinterpret_cast<uint8_t*>(&header)) + 1, sizeof(header) - 1);
    sum.Accumulate(hb.Get_buffer());
    uint16_t sum_val = sum.Accumulate(hb.Get_extra_byte());
    mavlink::Uint16 mavlink_cksum = sum_val;
    message = message->Concatenate(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));

    decoder.Decode(Io_buffer::Create(&header, sizeof(header)));
    decoder.Decode(hb.Get_buffer());
    decoder.Decode(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));

    decoder.Decode(message);

    decoder.Register_handler(Make_mavlink_decoder_handler(&Mavlink_message_handler));

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
    sum.Reset();
    sum.Accumulate((reinterpret_cast<uint8_t*>(&header)) + 1, sizeof(header) - 1);
    sum.Accumulate(hb.Get_buffer());
    sum_val = sum.Accumulate(hb.Get_extra_byte());
    mavlink_cksum = sum_val;
    message = message->Concatenate(Io_buffer::Create(&mavlink_cksum, sizeof(mavlink_cksum)));

    decoder.Decode(message);
    CHECK_EQUAL(1, decoder.Get_stats().unknown_id);
}

