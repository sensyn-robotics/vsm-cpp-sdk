#include <ugcs/vsm/overlapped_io_controller.h>

using namespace ugcs::vsm::internal;

Overlapped_io_controller::Overlapped_io_controller()
{
    completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!completion_port) {
        VSM_SYS_EXCEPTION("Failed to create I/O completion port");
    }
}

Overlapped_io_controller::~Overlapped_io_controller()
{
    CloseHandle(completion_port);
}

void
Overlapped_io_controller::Enable()
{
    dispatcher_thread = std::thread(&Overlapped_io_controller::Dispatcher_thread,
                                    this);
}

void
Overlapped_io_controller::Disable()
{
    /* Signal dispatcher to exit. */
    PostQueuedCompletionStatus(completion_port, 0, 0, nullptr);
    dispatcher_thread.join();
}

void
Overlapped_io_controller::Register_handle(
    File_processor::Stream::Native_handle &handle)
{
    Windows_file_handle &win_handle =
        reinterpret_cast<Windows_file_handle &>(handle);

    if (!CreateIoCompletionPort(win_handle.handle, completion_port,
                                reinterpret_cast<ULONG_PTR>(&handle), 0)) {

        VSM_SYS_EXCEPTION("Failed to attach handle to I/O completion port");
    }

    if (win_handle.write_handle != INVALID_HANDLE_VALUE &&
        !CreateIoCompletionPort(win_handle.write_handle, completion_port,
                                reinterpret_cast<ULONG_PTR>(&handle), 0)) {

        VSM_SYS_EXCEPTION("Failed to attach handle to I/O completion port");
    }
}

void
Overlapped_io_controller::Unregister_handle(
    File_processor::Stream::Native_handle &handle __UNUSED)
{
    /* No special actions required - completion port tracks handles references. */
}

void
Overlapped_io_controller::Dispatcher_thread()
{
    while (true) {
        DWORD transfer_size;
        ULONG_PTR key = 0;
        LPOVERLAPPED io_cb;
        DWORD error = 0;
        if (!GetQueuedCompletionStatus(completion_port, &transfer_size, &key,
                                       &io_cb, INFINITE) && key) {

            error = GetLastError();
        }
        if (!key) {
            /* Indicates exit request. */
            break;
        }
        reinterpret_cast<Windows_file_handle *>(key)->
            Io_complete_cbk(io_cb, transfer_size, error);
    }
}
