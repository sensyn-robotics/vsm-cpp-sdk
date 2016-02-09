// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Singleton class.
 */

#include <ugcs/vsm/transport_detector.h>

#include <UnitTest++.h>
#include <future>

using namespace ugcs::vsm;


TEST(vsm_proxy)
{
    auto sp = Socket_processor::Get_instance();
    auto td = Transport_detector::Get_instance();
    auto proccer = Request_processor::Create("UT2 vsm_proxy");
    auto worker = Request_worker::Create("UT1 vsm_proxy", std::initializer_list<ugcs::vsm::Request_container::Ptr>{proccer});
    auto pr = Properties::Get_instance();
    Timer_processor::Get_instance()->Enable();
    sp->Enable();
    worker->Enable();
    proccer->Enable();
    td->Enable();

    Socket_processor::Socket_listener::Ref listener;
    Socket_processor::Socket_listener::Ref server;

    std::promise<void> detected;

    // My transport-connected callback.
    auto h = [&](
        std::string,
        int,
        ugcs::vsm::Socket_address::Ptr,
        ugcs::vsm::Io_stream::Ref,
        int instance)
    {
        LOG("got connection for %d", instance);
        if (instance == 2) {
            detected.set_value();
        }
    };

    // Set up dummy configuration
    pr->Set("p.1.proxy","127.0.0.1");
    pr->Set("p.1.port","12345");

    // Simulate proxy endpoint.
    sp->Listen("127.0.0.1", "12345",
            Make_socket_listen_callback(
                [&](Socket_processor::Stream::Ref l, Io_result)
                {
                    listener = l;
                }));

    // Add two detectors for the proxy connection
    td->Add_detector(
            "p",
            Transport_detector::Make_connect_handler(h, 1),
            proccer);

    td->Add_detector(
            "p",
            Transport_detector::Make_connect_handler(h, 2),
            proccer);

    // Simulate proxy first time.
    sp->Accept(
        listener,
        Make_socket_accept_callback(
            [&](Socket_processor::Stream::Ref s, Io_result)
            {
                server = s;
            }));

    // Respond with success to eventual transport detector connection.
    // Should trigger first transport-connected callback.
    server->Write(Io_buffer::Create("VSMP\x02"));

    // Simulate proxy second time.
    sp->Accept(
        listener,
        Make_socket_accept_callback(
            [&](Socket_processor::Stream::Ref s, Io_result)
            {
                server = s;
            }));

    // Respond with invalid data. detector should establish new connection within a second.
    server->Write(Io_buffer::Create("warez"));

    // Simulate proxy third time.
    sp->Accept(
        listener,
        Make_socket_accept_callback(
            [&](Socket_processor::Stream::Ref s, Io_result)
            {
                server = s;
            }));

    // Should trigger second transport-connected callback and succeed the test.
    server->Write(Io_buffer::Create("VSMP\x02"));

    // Wait for test completion.
    auto ret = detected.get_future().wait_for(std::chrono::seconds(5));

    // Test the test result.
    CHECK (ret == std::future_status::ready);

    // Cleanup
    listener->Close();
    server->Close();
    td->Disable();
    proccer->Disable();
    worker->Disable();
    sp->Disable();
    Timer_processor::Get_instance()->Disable();
}

