// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <vsm/mavlink_demuxer.h>

using namespace vsm;

mavlink::MESSAGE_ID_TYPE def_msg_id;
mavlink::System_id def_sys_id;
uint8_t def_com_id;
bool resubmit;
bool hb_handler_called;

void
Heartbeat_handler(mavlink::Message<mavlink::MESSAGE_ID::HEARTBEAT>::Ptr message)
{
    def_msg_id = message->payload.Get_id();
    def_sys_id = message->Get_sender_system_id();
    def_com_id = message->Get_sender_component_id();
    hb_handler_called = true;
}

bool
Default_handler(mavlink::MESSAGE_ID_TYPE message_id, mavlink::System_id system_id,
                uint8_t component_id, Mavlink_demuxer* demuxer)
{
    if (message_id == mavlink::MESSAGE_ID::HEARTBEAT) {
        demuxer->Register_handler<mavlink::MESSAGE_ID::HEARTBEAT, mavlink::Extension>(
                Make_mavlink_demuxer_handler<
                mavlink::MESSAGE_ID::HEARTBEAT,
                mavlink::Extension>(Heartbeat_handler),
                system_id, component_id);
        return resubmit;
    }
    return resubmit;
}

TEST(basic_test)
{
    Mavlink_demuxer demuxer;

    /* Nothing done. */
    CHECK(!demuxer.Demux(nullptr, mavlink::MESSAGE_ID::HEARTBEAT, 0, 0));

    demuxer.Register_default_handler
        (Make_mavlink_demuxer_default_handler(Default_handler, &demuxer));

    Io_buffer::Ptr buffer = Io_buffer::Create("crap");

    CHECK(!demuxer.Demux(buffer, mavlink::MESSAGE_ID::MISSION_COUNT, 0, 0));

    /* Resubmit should fail demuxing on specific handler, because buffer is crap. */
    resubmit = true;
    CHECK_THROW(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 0, 0),
                Invalid_param_exception);

    /* Demuxing shouldn't fail, because specific handler is not called. */
    demuxer.Unregister_handler();
    resubmit = false;
    /* Should not throw any exceptions. */
    demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 0, 0);

    mavlink::Pld_heartbeat hb;
    buffer = Io_buffer::Create(&hb, sizeof(hb));

    hb_handler_called = false;
    CHECK(!demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 1, 0));
    /* Not called, because resubmit is false and different system it provided. */
    CHECK(!hb_handler_called);

    hb_handler_called = false;
    CHECK(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 0, 0));
    /* Called, because HB handler for (0,0) is already registered. */
    CHECK(hb_handler_called);

    hb_handler_called = false;
    CHECK(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 1, 0));
    /* Called, because HB handler for (1,0) is already registered. */
    CHECK(hb_handler_called);

    hb_handler_called = false;
    resubmit = true;
    CHECK(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 2, 3));
    /* Called, because HB handler for (2,3) should be registered and
     * immediately called due to resubmit.
     */
    CHECK(hb_handler_called);
    CHECK(mavlink::MESSAGE_ID::HEARTBEAT == def_msg_id);
    CHECK_EQUAL(2, def_sys_id);
    CHECK_EQUAL(3, def_com_id);

    /* Unregister all from system id 2. */
    demuxer.Unregister_handler(2, Mavlink_demuxer::MESSAGE_ID_ANY,
                               Mavlink_demuxer::COMPONENT_ID_ANY);
    hb_handler_called = false;
    resubmit = false;
    CHECK(!demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 2, 3));
    CHECK(!hb_handler_called);

    hb_handler_called = false;
    CHECK(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 1, 0));
    /* Called, because HB handler for (1,0) is still registered. */
    CHECK(hb_handler_called);
    CHECK(mavlink::MESSAGE_ID::HEARTBEAT == def_msg_id);
    CHECK_EQUAL(1, def_sys_id);
    CHECK_EQUAL(0, def_com_id);

    /* Unregister all. */
    demuxer.Unregister_handler(Mavlink_demuxer::SYSTEM_ID_ANY,
                               Mavlink_demuxer::MESSAGE_ID_ANY,
                               Mavlink_demuxer::COMPONENT_ID_ANY);
    hb_handler_called = false;
    CHECK(!demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 1, 0));
    /* Not called, because all handlers were unregistered. */
    CHECK(!hb_handler_called);

    demuxer.Disable();
}

TEST(handler_from_dedicated_processor)
{
    Mavlink_demuxer demuxer;

    Request_processor::Ptr processor = Request_processor::Create("UT mavlink handler_from_dedicated_processor");
    processor->Enable();

    demuxer.Register_handler<mavlink::MESSAGE_ID::HEARTBEAT, mavlink::Extension>(
                    Make_mavlink_demuxer_handler<
                    mavlink::MESSAGE_ID::HEARTBEAT,mavlink::Extension>
                        (Heartbeat_handler),
                    42, 43, processor);

    mavlink::Pld_heartbeat hb;
    Io_buffer::Ptr buffer = Io_buffer::Create(&hb, sizeof(hb));

    hb_handler_called = false;
    CHECK(demuxer.Demux(buffer, mavlink::MESSAGE_ID::HEARTBEAT, 42, 43));
    CHECK(!hb_handler_called);
    processor->Process_requests();
    CHECK(hb_handler_called);

    processor->Disable();
    demuxer.Disable();

}

TEST(handler_key_reg_unreg)
{
    Mavlink_demuxer demuxer;

    for (int i = 0; i < 2; i++) {
        auto key = demuxer.Register_handler
                <mavlink::MESSAGE_ID::HEARTBEAT, mavlink::Extension>(
                        Make_mavlink_demuxer_handler
                        <mavlink::MESSAGE_ID::HEARTBEAT,mavlink::Extension>
                        (Heartbeat_handler),
                        42, 43);

        demuxer.Unregister_handler(key);
    }
}
