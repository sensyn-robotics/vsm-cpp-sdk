// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Singleton class.
 */

#include <ugcs/vsm/transport_detector.h>
#include <ugcs/vsm/param_setter.h>

#include <UnitTest++.h>
#include <future>
#include <thread>

using namespace ugcs::vsm;


TEST(udp_multistream)
{
    auto sp = Socket_processor::Get_instance();
    auto td = Transport_detector::Get_instance();
    auto proccer = Request_processor::Create("UT processor");
    auto worker = Request_worker::Create("UT thread", std::initializer_list<ugcs::vsm::Request_container::Ptr>{proccer});
    auto pr = Properties::Get_instance();
    Timer_processor::Get_instance()->Enable();
    sp->Enable();
    worker->Enable();
    proccer->Enable();
    td->Enable();

    Socket_processor::Socket_listener::Ref client1_stream;
    Socket_processor::Socket_listener::Ref client2_stream;
    Socket_processor::Socket_listener::Ref broadcast_stream;
    Socket_processor::Socket_listener::Ref unicast_stream;
    Socket_processor::Socket_listener::Ref anycast_stream;

    Io_result client_result;
    Io_buffer::Ptr client_buf;

    std::promise<Io_stream::Ref> accepted1;
    std::promise<Io_stream::Ref> accepted2;

    auto client = Socket_address::Create("0.0.0.0", "0");
    auto broadcast = Socket_address::Create("127.255.255.255", "6666");
    auto unicast = Socket_address::Create("127.0.0.1", "6666");
    auto anycast = Socket_address::Create("0.0.0.0", "6666");

    // Create two remote client sockets.
    sp->Bind_udp(client, Make_setter(client1_stream, client_result));
    sp->Bind_udp(client, Make_setter(client2_stream, client_result));
    client1_stream->Enable_broadcast(true);
    client2_stream->Enable_broadcast(true);

    // packet counters for each client.
    int readcount1 = 0;
    int readcount2 = 0;
    int readcount3 = 0;

    // wait for 50 reads before exit.
    std::mutex m;
    std::condition_variable cv;

    // Create server listener.
    sp->Bind_udp(anycast, Make_setter(anycast_stream, client_result));

    // Reader callback issues next read automatically.
    std::function<void(Io_buffer::Ptr, Io_result, Socket_address::Ptr, Socket_processor::Stream::Ref, int*)> reader;
    reader = [&](
            Io_buffer::Ptr buf,
            Io_result r,
            ugcs::vsm::Socket_address::Ptr peer,
            Socket_processor::Stream::Ref s,
            int* counter)
    {
        if (r == Io_result::OK) {
            LOG("Read on %s from %s: %s", s->Get_local_address()->Get_as_string().c_str(), peer->Get_as_string().c_str(), buf->Get_ascii().c_str());
            std::unique_lock<std::mutex> lk(m);
            (*counter)++;
            lk.unlock();
            cv.notify_one();
            s->Read_from(20, Make_socket_read_from_callback(reader, s, counter), worker);
        } else {
            LOG("Read failed %d on %s", static_cast<int>(r), s->Get_name().c_str());
        }
    };

    // Accept two connections.
    sp->Accept(
            anycast_stream,
            Make_socket_accept_callback([&](Socket_processor::Stream::Ref s, Io_result r){
                if (r == Io_result::OK) {
                    LOG("Accepted1 %s", s->Get_name().c_str());
                    s->Read_from(20, Make_socket_read_from_callback(reader, s, &readcount1), worker);
                    accepted1.set_value(s);
                } else {
                    LOG("Accept1 failed %d", static_cast<int>(r));
                }
            }),
            worker);

    sp->Accept(
            anycast_stream,
            Make_socket_accept_callback([&](Socket_processor::Stream::Ref s, Io_result r){
                if (r == Io_result::OK) {
                    LOG("Accepted2 %s", s->Get_name().c_str());
                    s->Read_from(20, Make_socket_read_from_callback(reader, s, &readcount2), worker);
                    accepted2.set_value(s);
                } else {
                    LOG("Accept2 failed %d", static_cast<int>(r));
                }
            }),
            worker);

    // Use broadcast to trigger the two acceptors and a read for each.
    client1_stream->Write_to(Io_buffer::Create("hello from c1"), broadcast, Make_setter(client_result));
    client2_stream->Write_to(Io_buffer::Create("hello from c2"), broadcast, Make_setter(client_result));

    auto f1 = accepted1.get_future();
    auto f2 = accepted2.get_future();
    auto future1_status = f1.wait_for(std::chrono::seconds(3));
    auto future2_status = f2.wait_for(std::chrono::seconds(3));
    CHECK(future1_status == std::future_status::ready);
    CHECK(future2_status == std::future_status::ready);
    if (    future1_status != std::future_status::ready
        ||  future2_status != std::future_status::ready) {
        return;
    }

    auto accepted1_stream = f1.get();
    auto accepted2_stream = f2.get();

    // trigger another read on both accepted streams
    client1_stream->Write_to(Io_buffer::Create("c1"), unicast, Make_setter(client_result));
    client2_stream->Write_to(Io_buffer::Create("c2"), unicast, Make_setter(client_result));

    // Close one stream.
    accepted2_stream->Close();

    // Unread writes to one stream should not block reads from other stream on the same socket.
    for (int i = 0; i < 1000; i++) {
        client2_stream->Write_to(
            Io_buffer::Create(std::to_string(i)),
            broadcast, Make_setter(client_result));
    }
    client1_stream->Write_to(Io_buffer::Create("c1"), unicast, Make_setter(client_result));

    anycast_stream->Read_from(20, Make_socket_read_from_callback(reader, anycast_stream, &readcount3), worker);

    std::unique_lock<std::mutex> lk(m);

    // Wait for Stream::MAX_CACHED_COUNT reads.
    cv.wait_for(lk, std::chrono::seconds(3), [&]{return readcount3 == 50;});

    readcount2 = 0;
    accepted1_stream->Write(Io_buffer::Create("writeback"), Make_setter(client_result));

    client1_stream->Read_from(20, Make_socket_read_from_callback(reader, client1_stream, &readcount2), worker);

    // Wait for the read to complete.
    cv.wait_for(lk, std::chrono::seconds(3), [&]{return readcount2 == 1;});

    // There must be 3 successful reads on accepted1_stream.
    CHECK(readcount1 == 3);
    CHECK(readcount2 == 1);
    CHECK(readcount3 == 50);

    accepted1_stream->Close();
    accepted2_stream->Close();
    client1_stream->Close();
    client2_stream->Close();
    anycast_stream->Close();

    td->Disable();
    proccer->Disable();
    worker->Disable();
    sp->Disable();
    Timer_processor::Get_instance()->Disable();
}
