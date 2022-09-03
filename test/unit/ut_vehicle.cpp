// Copyright (c) 2018, Smart Projects Holdings Ltd
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
    // LOG("Sent msg=%s", message.DebugString().c_str());
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
        // LOG("Got msg=%s", vsm_msg.DebugString().c_str());
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
        some_prop(some_prop)
    {
        Set_vehicle_type(proto::VEHICLE_TYPE_MULTICOPTER);
        Set_autopilot_type("myvehicle");
        Set_model_name("SuperCopter");
        Set_serial_number("123456");
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
    v->Register();
    CHECK_EQUAL(10, v->some_prop);

    Socket_processor::Ptr sp = ugcs::vsm::Socket_processor::Get_instance();

    // Wait for ucs listener to appear.
    auto result = Io_result::CONNECTION_REFUSED;
    while (result == Io_result::CONNECTION_REFUSED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto w = sp->Connect("127.0.0.1", "5556",
                Make_socket_connect_callback([&](Socket_processor::Stream::Ref s, Io_result res ) {
            result = res;
            ucs_stream = s;
        }));
        w.Wait();
    }

    CHECK(Io_result::OK == result);

    // send server hello message
    ugcs::vsm::proto::Vsm_message vsm_msg;
    auto r = vsm_msg.mutable_register_peer();
    r->set_peer_id(111222);
    // simulate server has the same version as VSM.
    r->set_version_major(SDK_VERSION_MAJOR);
    r->set_version_minor(SDK_VERSION_MINOR);
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

TEST(get_takeoff_altitude_was_armed)
{
    // Test takeOffAltitude is 500.0
    bool was_armed = true;
    std::string route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\":500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    std::optional<double> takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK_EQUAL(500.0, takeoff_altitude.value());

    // Test takeOffAltitude is 600.5
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\":600.5}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK_EQUAL(600.5, takeoff_altitude.value());

    // Test takeOffAltitude is 0.0
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\":0.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK_EQUAL(0.0, takeoff_altitude.value());
}

TEST(get_takeoff_altitude_not_armed)
{
    // Test not armed
    bool was_armed = false;
    std::string route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\":500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    std::optional<double> takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK(std::nullopt == takeoff_altitude);
}

TEST(get_takeoff_altitude_json_format)
{
    // Test takeOffAltitude json str have blank character
    bool was_armed = true;
    std::string route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\": 500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    std::optional<double> takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK_EQUAL(500.0, takeoff_altitude.value());

    // Test takeOffAltitude json str have int value
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOffAltitude\": 500}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK_EQUAL(500.0, takeoff_altitude.value());

    // Test takeOffAltitude json str have a wrong key
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeOff-Altitude\": 500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK(std::nullopt == takeoff_altitude);

    // Test takeOffAltitude json str have a wrong case-sensitive key(lowercase)
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += '\0';
    route_name += "{\"takeoffaltitude\": 500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK(std::nullopt == takeoff_altitude);

    // Test takeOffAltitude json str have no Embedded null character in route_name
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    route_name += "{\"takeOffAltitude\": 500.0}";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK(std::nullopt == takeoff_altitude);

    // Test have no takeOffAltitude json str in route_name
    was_armed = true;
    route_name = "0-M300RTK-xxxxxxxx";
    LOG("Embedded null character route_name: %s", route_name.c_str());
    takeoff_altitude = Vehicle::Get_takeoff_altitude(was_armed, route_name);
    CHECK(std::nullopt == takeoff_altitude);
}
