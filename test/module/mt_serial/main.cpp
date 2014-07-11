// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Module testing utility for serial port.
 *
 * It acts just as echo responder on the specified serial port.
 */

#include <ugcs/vsm/vsm.h>
#include <ugcs/vsm/serial_processor.h>

#include <cstdio>

#include <signal.h>

using namespace ugcs::vsm;

const char usage[] = "\
Usage: \n\
%s <serial port name> \n\
";

/** Port name argument. */
const char *port_name;

volatile bool exiting = false;

void
Read_handler(Io_buffer::Ptr data, Io_result result, Io_stream::Ref stream,
             Request_worker::Ptr worker)
{
    LOG("[%luB res %d] %s\n", data ? data->Get_length() : 0, result,
        data ? data->Get_string().c_str() : "<NULL>");

    if (result == Io_result::OK) {
        /* Write echo response synchronously. */
        stream->Write(Io_buffer::Create("Echo: ")->Concatenate(data));
    }

    /* Start next reading. */
    if (!exiting) {
        stream->Read(512, 1, Make_read_callback(Read_handler, stream, worker), worker);
    }
}

void
Handle_int(int signal __UNUSED)
{
    exiting = true;
    printf("Interrupt requested, exiting...\n");
}

void
Main(Serial_processor::Ptr proc, Request_worker::Ptr worker)
{
    Io_stream::Ref stream = proc->Open(port_name, Serial_processor::Stream::Mode().
                                       Baud(115200));

    /* Start initial reading. */
    stream->Read(512, 1, Make_read_callback(Read_handler, stream, worker), worker);

    while (!exiting) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    stream->Close();
}

int
main (int argc, char *argv[])
{
    if (argc != 2) {
        std::printf(usage, argv[0]);
        return 1;
    }
    port_name = argv[1];
    printf("Opening port \"%s\"\n", port_name);

#   ifdef __unix__
    /* Exit after SIGINT. */
    struct sigaction sa;
    sa.sa_handler = &Handle_int;
    sa.sa_flags = SA_RESETHAND;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
#   endif

    ugcs::vsm::Initialize();

    Serial_processor::Ptr proc = Serial_processor::Create();
    proc->Enable();

    Request_worker::Ptr worker = Request_worker::Create("MT serial worker");
    worker->Enable();

    Main(proc, worker);

    worker->Disable();
    proc->Disable();

    ugcs::vsm::Terminate();
    return 0;
}
