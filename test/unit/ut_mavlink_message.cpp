// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Unit tests for MAVLink messages parser/serializer. */

#include <ugcs/vsm/mavlink.h>
#include <UnitTest++.h>

using namespace ugcs::vsm;
using namespace ugcs::vsm::mavlink;

/* Define plain format of some message under test. */
struct Mav_param_request_read {
    int16_t param_index;
    uint8_t target_system;
    uint8_t target_component;
    char param_id[16];
};

void
Check_message(Pld_param_request_read &msg, uint8_t target_system,
              uint8_t target_component, const char *param_id,
              int16_t param_index)
{
    auto buf = msg.Get_buffer();
    auto msg_buf = static_cast<const Mav_param_request_read *>(buf->Get_data());

    CHECK_EQUAL(target_system, msg_buf->target_system);
    CHECK_EQUAL(target_component, msg_buf->target_component);
    CHECK(msg->param_id == param_id);
    CHECK_EQUAL(Le(param_index), msg_buf->param_index);
}

TEST(basic_functionality)
{
    CHECK_EQUAL(sizeof(Mav_param_request_read),
                Pld_param_request_read::Get_payload_size());

    Mav_param_request_read msg_buf = {Le(static_cast<int16_t>(0x0102)),
                                      1, 2, "param ID"};

    Pld_param_request_read msg(&msg_buf, sizeof(msg_buf));

    CHECK_EQUAL(1, msg->target_system);
    CHECK_EQUAL(2, msg->target_component);
    CHECK(msg->param_id == "param ID");
    CHECK(msg->param_id.Get_string() == "param ID");
    CHECK_EQUAL(0x0102, msg->param_index);
    CHECK_EQUAL(0x0102, msg->param_index.Get());

    /* Assign to multi-byte value. */
    msg->param_index = 0x0304;
    Check_message(msg, 1, 2, "param ID", 0x0304);
    msg->target_system = 5;
    Check_message(msg, 5, 2, "param ID", 0x0304);
    /* Assign string to characters array. */
    msg->param_id = "012345678901234";
    Check_message(msg, 5, 2, "012345678901234", 0x0304);
    msg->param_id = "0123456789012345";
    Check_message(msg, 5, 2, "0123456789012345", 0x0304);
    /* Last character should be truncated. */
    msg->param_id = "01234567890123456";
    Check_message(msg, 5, 2, "0123456789012345", 0x0304);

    /* Check dumping. */
    std::string str = msg.Dump();
    const char *expected_str =
        "Message PARAM_REQUEST_READ (20 bytes)\n"
        "int16_t param_index: 772\n"
        "uint8_t target_system: 5\n"
        "uint8_t target_component: 2\n"
        "char param_id[16]: '0123456789012345'\n";
    CHECK_EQUAL(expected_str, str.c_str());

    /* Check reset. */
    CHECK(!msg->param_index.Is_reset());
    msg->param_index.Reset(); /* For int. */
    CHECK_EQUAL(std::numeric_limits<int16_t>::max(), msg->param_index);
    CHECK(msg->param_index.Is_reset());

    ::ugcs::vsm::mavlink::ugcs::Pld_mission_item_ex mi;
    CHECK(!mi->param2.Is_reset()); /* For float. */
    mi->param2.Reset();
    CHECK(mi->param2.Is_reset());
    CHECK(std::isnan(mi->param2));
}

TEST(version_field)
{
    /* Check that version field is initialized automatically to current protocol
     * version.
     */
    Pld_heartbeat msg;
    CHECK_EQUAL(VERSION, msg->mavlink_version);
}
