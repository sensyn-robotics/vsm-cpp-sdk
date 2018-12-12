// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/param_setter.h>
#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/vsm.h>
#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/debug.h>

#include <iostream>

#include <UnitTest++.h>

using namespace ugcs::vsm;
using namespace std;

class Test_case_wrapper
{
public:
    Test_case_wrapper() {
        ugcs::vsm::Initialize("vsm.conf");
    }

    ~Test_case_wrapper() {
        ugcs::vsm::Terminate();
    }
};


TEST_FIXTURE(Test_case_wrapper, socket_processor_udp)
{
    Socket_processor::Ptr sp = ugcs::vsm::Socket_processor::Get_instance();
    Request_worker::Ptr worker = Request_worker::Create("UT socket processor worker");
    CHECK(sp->Is_enabled());
    CHECK(!worker->Is_enabled());
    worker->Enable();
    CHECK(worker->Is_enabled());

    Socket_processor::Socket_listener::Ref server_stream;
    Socket_processor::Stream::Ref client_stream;
    Io_result server_result;
    Io_result client_result;

    Io_buffer::Ptr client_buf;
    Io_buffer::Ptr server_buf;

    // forward declaration
    std::function<void(Io_buffer::Ptr, Io_result, Socket_address::Ptr)> server_read_complete;
    Socket_address::Ptr bound_client_point = nullptr;

    std::function<void(Io_result)>
    server_write_complete = [&](Io_result result)
    {
        if (result != Io_result::OK) {
            std::cout<<"WRITE NOK"<<std::endl;
        } else {
            // initiate next read after write;
            server_stream->Read_from(1000, Make_callback(server_read_complete, server_buf, server_result, bound_client_point), worker);
        }
    };

    // write back into socket everything received.
    server_read_complete = [&](Io_buffer::Ptr buffer, Io_result result, Socket_address::Ptr peer)
    {
        if (result != Io_result::OK) {
            server_stream->Close();
        } else {
            // echo back
            server_stream->Write_to(buffer, peer, Make_callback(server_write_complete, server_result), worker);
        }
    };

    auto server_point = Socket_address::Create("127.0.0.1", "32769");
    sp->Connect(server_point,
            Make_socket_listen_callback([&](Socket_processor::Stream::Ref l, Io_result){
            client_stream = l;
            }),
            Request_temp_completion_context::Create(),
            Io_stream::Type::UDP);

    // Create serverside socket.
    sp->Bind_udp(server_point,
            Make_socket_listen_callback([&](Socket_processor::Stream::Ref l, Io_result){
            server_stream = l;
            server_stream->Read_from(1000, Make_callback(server_read_complete, server_buf, server_result, bound_client_point), worker);
            }));

    // Send data to server
    client_stream->Write(Io_buffer::Create("0123456789"), Make_setter(client_result));
    client_stream->Write(Io_buffer::Create("abcdefghij"), Make_setter(client_result));

    // Read back from server
    client_stream->Read(10, 10, Make_setter(client_buf, client_result));
    // Check the data
    CHECK_EQUAL("0123456789", client_buf->Get_string().c_str());

    // Read back from server
    client_stream->Read(100, 10, Make_setter(client_buf, client_result)).Timeout(std::chrono::milliseconds(1000));

    CHECK(client_result == Io_result::OK);
    // Check the data
    CHECK_EQUAL("abcdefghij", client_buf->Get_string().c_str());

    client_stream->Close();
    server_stream->Close();
    worker->Disable();
}

TEST_FIXTURE(Test_case_wrapper, socket_processor_listen_accept)
{
    Socket_processor::Ptr sp = ugcs::vsm::Socket_processor::Get_instance();
    Request_worker::Ptr worker = Request_worker::Create("UT socket processor worker");
    CHECK(sp->Is_enabled());
    CHECK(!worker->Is_enabled());
    worker->Enable();
    CHECK(worker->Is_enabled());

    Socket_processor::Socket_listener::Ref listener;
    Socket_processor::Stream::Ref client_stream;
    Socket_processor::Stream::Ref server_stream;
    Io_result server_result;
    Io_result client_result;

    Io_buffer::Ptr client_buf;
    Io_buffer::Ptr server_buf;

    // forward declaration
    std::function<void(Io_buffer::Ptr, Io_result, Io_stream::Ref)> server_read_complete;

    std::function<void(Io_result, Io_stream::Ref)>
    server_write_complete = [&](Io_result result, Io_stream::Ref s)
    {
        if (result != Io_result::OK) {
            std::cout<<"WRITE NOK"<<std::endl;
        } else {
            // initiate next read after write;
            s->Read(1000, 1, Make_callback(server_read_complete, server_buf, server_result, s), worker);
        }
    };

    // write back into socket everything received.
    server_read_complete = [&](Io_buffer::Ptr buffer, Io_result result, Io_stream::Ref stream)
    {
        if (result != Io_result::OK) {
//            std::cout<<"READ NOK"<<std::endl;
            stream->Close();
        } else {
            // echo back
//            std::cout<<"Read "<<buffer->Get_length()<<" bytes"<<std::endl;
            stream->Write(buffer, Io_stream::OFFSET_NONE, Make_callback(server_write_complete, server_result, stream), worker);
        }
    };

    // initiate
    std::function<void(Socket_processor::Stream::Ref, Io_result)>
    accept_complete = [&](Socket_processor::Stream::Ref s, Io_result)
    {
        if (s)
        {
            server_stream = s;
            auto addr = s->Get_peer_address();
            s->Read(1000, 1, Make_callback(server_read_complete, server_buf, server_result, s), worker);
        }
    };

    // Create listener socket. Do not provide compl context, the call will block until listener ready.
    sp->Listen("127.0.0.1", "12345",
            Make_socket_listen_callback([&](Socket_processor::Stream::Ref l, Io_result){
                listener = l;
            }));

    // Set up acceptor. Provide worker as completion context. The call will return immediately.
    sp->Accept(listener, Make_socket_accept_callback(accept_complete), worker);

    // Connect to the socket.
    // Do not provide completion context, the call will block until conection ready.
    sp->Connect("127.0.0.1", "12345",
            Make_socket_connect_callback([&](Socket_processor::Stream::Ref s, Io_result){
        client_stream = s;
    }));

//    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Send data to server
    client_stream->Write(Io_buffer::Create("0123456789"), Make_setter(client_result));

    // Try sending zerolen buffer
    client_stream->Write(Io_buffer::Create(), Make_setter(client_result));
    CHECK(Io_result::OK == client_result);

    // Send data to server
    client_stream->Write(Io_buffer::Create("abcdefghij"), Make_setter(client_result));

    // Read back from server
    client_stream->Read(10, 10, Make_setter(client_buf, client_result));
    // Check the data
    CHECK_EQUAL("0123456789", client_buf->Get_string().c_str());

    // Read zero bytes from server
    client_stream->Read(0, 0, Make_setter(client_buf, client_result));
    CHECK(Io_result::OK == client_result);

    client_stream->Read(0, 0, Make_setter(client_buf, client_result));
    CHECK(Io_result::OK == client_result);

    // Read back from server
    client_stream->Read(100, 10, Make_setter(client_buf, client_result)).Timeout(std::chrono::milliseconds(1000));

    CHECK(client_result == Io_result::OK);
    // Check the data
    CHECK_EQUAL("abcdefghij", client_buf->Get_string().c_str());

    // Abort on pending read should not block subsequent reads.
    // Pend read.
    auto read_op = client_stream->Read(100, 10, Make_setter(client_buf, client_result));
    // Let it some time to actually call select.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Abort the read op.
    read_op.Abort();
    // Write something to server (it will return it back and trigger the aborted read handler)
    client_stream->Write(Io_buffer::Create("abcdefghij"), Make_setter(client_result));
    // Read the result.
    client_stream->Read(100, 10, Make_setter(client_buf, client_result)).Timeout(std::chrono::seconds(1));
    CHECK(client_result == Io_result::OK);
    if (client_result == Io_result::OK) {
        // Check the data
        CHECK_EQUAL("abcdefghij", client_buf->Get_string().c_str());
    }

    client_stream->Close();
    listener->Close();
    server_stream->Close();
    worker->Disable();
}

/* Overflow write queue until write operations time out, then cancel them all. */
class Timed_writes
{
public:

    Timed_writes()
    {
        Initialize();
        worker = Request_worker::Create("UT timed writes");
        worker->Enable();

        sp = Socket_processor::Get_instance();

        /* Sync call. */
        sp->Listen(
                "127.0.0.1", "12345",
                Make_socket_listen_callback(
                        &Timed_writes::On_listening_started,
                        this));
        /* Sync call. */
        sp->Connect(
                "127.0.0.1", "12345",
                Make_socket_connect_callback(
                        &Timed_writes::On_connected,
                        this));

        static char buffer[1000];

        while(!timed_out) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
            /* Async call. */
            auto waiter = client_stream->Write(
                    Io_buffer::Create(buffer, sizeof(buffer)),
                    Make_write_callback(
                            &Timed_writes::Write_completed,
                            this),
                    worker);
            waiter.Timeout(
                    std::chrono::milliseconds(300),
                    Make_timeout_callback(
                            &Timed_writes::Write_timed_out,
                            this),
                    std::rand() & 1);
            ops.emplace_back(std::move(waiter));
        }
    }

    ~Timed_writes()
    {
        int done = 0, not_done = 0;
        for(auto& iter: ops) {
            if (iter.Is_done()) {
                done++;
            } else {
                not_done++;
            }
            iter.Abort();
        }
        client_stream->Close();
        server_stream->Close();
        listener->Close();
        worker->Disable();
        Terminate();
        LOG_INFO("Done [%d] Not done [%d]", done, not_done);
    }

    void
    On_listening_started(Socket_processor::Socket_listener::Ref listener,
            Io_result result)
    {
        CHECK(Io_result::OK == result);
        this->listener = listener;
        sp->Accept(
                listener,
                Make_socket_accept_callback(
                        &Timed_writes::On_incomming_connection,
                        this),
                worker);
    }

    void
    On_incomming_connection(Socket_processor::Stream::Ref s, Io_result result)
    {
        LOG_INFO("Incoming connection [%s]", s->Get_name().c_str());
        CHECK(result == Io_result::OK);
        server_stream = s;
    }

    void
    On_connected(Socket_processor::Stream::Ref s, Io_result result)
    {
        CHECK(result == Io_result::OK);
        client_stream = s;
    }

    void
    Write_completed(Io_result)
    {

    }

    void
    Write_timed_out(const Operation_waiter::Ptr&)
    {
        static int count;
        if (count++ > 1000) {
            timed_out = true;
        }
    }

    Socket_processor::Ptr sp;

    Request_worker::Ptr worker;

    std::list<Operation_waiter> ops;

    volatile bool timed_out = false;

    Socket_processor::Socket_listener::Ref listener;
    Socket_processor::Stream::Ref client_stream;
    Socket_processor::Stream::Ref server_stream;
};

TEST_FIXTURE(Timed_writes, timed_writes)
{
    /* No specific checks, it should just work, don't crash, don't assert anywhere. */
}

