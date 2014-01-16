// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/socket_processor.h>
#include <vsm/param_setter.h>
#include <vsm/request_worker.h>
#include <vsm/vsm.h>
#include <vsm/callback.h>
#include <vsm/debug.h>

#include <iostream>

#include <UnitTest++.h>

using namespace vsm;
using namespace std;

class Test_case_wrapper
{
public:
    Test_case_wrapper() {
        vsm::Initialize("vsm.conf", std::ios_base::in);
    }

    ~Test_case_wrapper() {
        vsm::Terminate();
    }
};

TEST_FIXTURE(Test_case_wrapper, socket_processor_listen_accept)
{
    Socket_processor::Ptr sp = vsm::Socket_processor::Get_instance();
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
            std::cout<<"READ NOK"<<std::endl;
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
            if (addr)
                std::cout << "Connection from: " << addr->Get_as_string() << std::endl;
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
    client_stream->Close();
    listener->Close();
    server_stream->Close();
    worker->Disable();
}
