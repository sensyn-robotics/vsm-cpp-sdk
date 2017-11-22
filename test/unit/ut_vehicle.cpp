// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Test for vehicle class.
 */

#include <ugcs/vsm/vsm.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;


Socket_processor::Stream::Ref ucs_stream;

void
Send_ucs_message(
    ugcs::vsm::proto::Vsm_message& message)
{
    message.set_device_id(111);

    auto payload_len = message.ByteSize();
    auto tmp_len = payload_len;
    int header_len = 0;
    std::vector<uint8_t> user_data(10 + payload_len);
    do {
        uint8_t byte = (tmp_len & 0x7f);
        tmp_len >>= 7;
        if (tmp_len) {
            byte |= 0x80;
        }
        user_data[header_len] = byte;
        header_len++;
    } while (tmp_len);
    message.SerializeToArray(user_data.data() + header_len, payload_len);
    user_data.resize(header_len + payload_len);
    Io_buffer::Ptr buffer = Io_buffer::Create(std::move(user_data));

    ucs_stream->Write(buffer);
}

void
Read_ucs_message(
    ugcs::vsm::proto::Vsm_message& vsm_msg)
{
    Io_buffer::Ptr ucs_buf;
    Io_result result;
    int len = 0;
    int byte;
    int shift = 0;
    do {
        ucs_stream->Read(1, 1, Make_setter(ucs_buf, result));
        byte = *static_cast<const uint8_t*>(ucs_buf->Get_data());
        len |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    ucs_stream->Read(len, len, Make_setter(ucs_buf, result));

    if (vsm_msg.ParseFromArray(ucs_buf->Get_data(), ucs_buf->Get_length())) {
        LOG("Got message len=%zu", ucs_buf->Get_length());
        //LOG("msg=%s", vsm_msg.DebugString().c_str());
    } else {
        LOG_ERR("ParseFromArray failed.");
    }
}

class Some_vehicle: public Vehicle
{
    DEFINE_COMMON_CLASS(Some_vehicle, Vehicle)
public:
    int some_prop;

    Some_vehicle(int some_prop):
        Vehicle(mavlink::MAV_TYPE::MAV_TYPE_QUADROTOR,
                mavlink::MAV_AUTOPILOT::MAV_AUTOPILOT_ARDUPILOTMEGA,
                Vehicle::Capabilities(),
                "123456", "SuperCopter"),
                some_prop(some_prop)
    {
    }

    void
    Test_ucs()
    {
        Reset_altitude_origin();

        c_auto->Set_enabled(true);
        c_auto->Set_available(true);
        t_latitude->Set_value(11.2);
        Commit_to_ucs();
    }
};

TEST(basic_usage)
{
    ugcs::vsm::Initialize("vsm.conf");

    auto v = Some_vehicle::Create(10);
    v->Enable();
    CHECK_EQUAL(10, v->some_prop);

    Socket_processor::Ptr sp = ugcs::vsm::Socket_processor::Get_instance();
    sp->Connect("127.0.0.1", "5556",
            Make_socket_connect_callback([&](Socket_processor::Stream::Ref s, Io_result){
        ucs_stream = s;
    }));

    // send server hello message
    ugcs::vsm::proto::Vsm_message vsm_msg;
    vsm_msg.mutable_register_peer()->set_peer_id(111222);
    Send_ucs_message(vsm_msg);

    // read vsm hello
    Read_ucs_message(vsm_msg);

    // read device register
    Read_ucs_message(vsm_msg);

    // Send Device_register response
    uint32_t id = vsm_msg.message_id();
    vsm_msg.Clear();
    vsm_msg.set_message_id(id);
    vsm_msg.mutable_device_response()->set_code(proto::STATUS_OK);
    Send_ucs_message(vsm_msg);

    // send some telemetry
    v->Test_ucs();

    // receive telemetry
    Read_ucs_message(vsm_msg);

    ucs_stream->Close();

    /* Implicit cast to base pointer works. */
    Vehicle::Ptr ptr = v;
    CHECK(ptr == v);
    v->Disable();
    ugcs::vsm::Terminate();
}

