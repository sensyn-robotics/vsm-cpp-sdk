// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <UnitTest++.h>
#include <vsm/vsm.h>

using namespace vsm;

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
