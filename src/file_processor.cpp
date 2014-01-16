// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * File_processor implementation.
 */

#include <vsm/file_processor.h>
#include <vsm/debug.h>

using namespace vsm;

/** define this to enable file locking specific logging.
 */
#define LLOG(...) //LOG(__VA_ARGS__)

/* ****************************************************************************/
/* File_processor::Stream class. */

File_processor::Stream::Mode::Mode(const std::string &mode_str)
{
    read = false;
    write = false;
    extended = false;
    should_not_exist = false;

    /* The first character should specify read or write access. */
    if (mode_str[0] == 'r') {
        read = true;
    } else if (mode_str[0] == 'w') {
        write = true;
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Invalid access type, should be "
                      "either 'r' or 'w'");
    }
    if (mode_str[1] == 0) {
        return;
    }
    size_t idx = 1;
    if (mode_str[1] == '+') {
        extended = true;
        idx++;
    }
    if (mode_str.size() == idx) {
        return;
    } else if (mode_str[idx] == 'x') {
        should_not_exist = true;
        idx++;
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Unexpected character at offset %lu, "
                      "expected either '+' or 'x'", idx);
    }
    if (mode_str.size() != idx) {
        VSM_EXCEPTION(Invalid_param_exception, "Unexpected trailing garbage found");
    }
    return;
}

File_processor::Stream::Stream(
    File_processor::Ptr processor, const std::string &path, Mode mode, bool maintain_pos):

    processor(processor), mode(mode), maintain_pos(maintain_pos),
    native_handle(Native_handle::Create(path, mode))
{
    Set_name(path);
    state = State::OPENED;
}

File_processor::Stream::Stream(Native_handle::Envelope *handle,
                               Native_handle::Envelope *write_handle,
                               File_processor::Ptr processor, const std::string &path,
                               Mode mode, bool maintain_pos):
    processor(processor), mode(mode), maintain_pos(maintain_pos),
    native_handle(Native_handle::Create(path, mode, handle, write_handle))
{
    Set_name(path);
    state = State::OPENED;
}

Operation_waiter
File_processor::Stream::Write_impl(Io_buffer::Ptr buffer,
                                   Offset offset,
                                   Write_handler completion_handler,
                                   Request_completion_context::Ptr comp_ctx)
{
    /* Use processor completion context if none is provided. */
    if (!comp_ctx) {
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }
        comp_ctx = proc->comp_ctx;
    }

    /* Use dummy completion handler, if none is provided. */
    if (!completion_handler) {
        completion_handler = Make_dummy_callback<void, Io_result>();
    }

    std::unique_lock<std::mutex> lock(op_mutex);

    auto this_ptr = Shared_from_this();

    Write_request::Ptr request = Write_request::Create(buffer, this_ptr,
                                                       offset,
                                                       completion_handler.template Get_arg<0>());
    auto comp_handler =
        Make_callback(&File_processor::Stream::Handle_write_completion,
                      this_ptr, completion_handler);
    request->Set_completion_handler(comp_ctx, comp_handler);
    auto proc_handler =
        Make_callback(&File_processor::Stream::Handle_write, this_ptr);
    request->Set_processing_handler(proc_handler);
    auto cancel_handler =
        Make_callback(&File_processor::Stream::Handle_write_cancel, this_ptr, request);
    request->Set_cancellation_handler(cancel_handler);
    Queue_write(request);
    return request;
}

Operation_waiter
File_processor::Stream::Read_impl(size_t max_to_read, size_t min_to_read,
                                  Offset offset,
                                  Read_handler completion_handler,
                                  Request_completion_context::Ptr comp_ctx)
{
    /* Use processor completion context if none is provided. */
    if (!comp_ctx) {
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }
        comp_ctx = proc->comp_ctx;
    }

    /* Use dummy completion handler, if none is provided. */
    if (!completion_handler) {
        completion_handler = Make_dummy_callback<void, Io_buffer::Ptr, Io_result>();
    }

    std::unique_lock<std::mutex> lock(op_mutex);

    auto this_ptr = Shared_from_this();

    Read_request::Ptr request =
        Read_request::Create(completion_handler.template Get_arg<0>(),
                             max_to_read, min_to_read, this_ptr, offset,
                             completion_handler.template Get_arg<1>());
    auto comp_handler =
        Make_callback(&File_processor::Stream::Handle_read_completion,
                      this_ptr, completion_handler);
    request->Set_completion_handler(comp_ctx, comp_handler);
    auto proc_handler =
        Make_callback(&File_processor::Stream::Handle_read, this_ptr);
    request->Set_processing_handler(proc_handler);
    auto cancel_handler =
        Make_callback(&File_processor::Stream::Handle_read_cancel, this_ptr, request);
    request->Set_cancellation_handler(cancel_handler);
    Queue_read(request);
    return request;
}

Operation_waiter
File_processor::Stream::Lock(   Lock_handler completion_handler,
                                Request_completion_context::Ptr comp_ctx,
                                bool do_lock)
{
    /* Use processor completion context if none is provided. */
    if (!comp_ctx) {
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }
        comp_ctx = proc->comp_ctx;
    }

    /* Use dummy completion handler, if none is provided. */
    if (!completion_handler) {
        completion_handler = Make_dummy_callback<void, Io_result>();
    }

    std::unique_lock<std::mutex> lock(op_mutex);

    auto this_ptr = Shared_from_this();

    Io_request::Ptr request = Io_request::Create(
            this_ptr,
            OFFSET_NONE,
            completion_handler.template Get_arg<0>());

    /** Allow only one outstanding lock/unlock request per stream.
     * 1) Lock request is valid only when there is
     * neither lock nor unlock request pending.
     * 2) Unlock request is valid only when there is no unlock
     * request pending. (lock request can be pending)
     */
    if (    native_handle->cur_unlock_request
        ||  (do_lock && native_handle->cur_lock_request))
    {
        lock.unlock();
        // Complete request immediately.
        request->Set_processing_handler(
                Make_callback([](Request::Ptr r){r->Complete();}, request));
        request->Set_completion_handler(comp_ctx, completion_handler);
        completion_handler.Set_args(Io_result::LOCK_ERROR);
        request->Process(true);
    } else {
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }

        // Completion handler is the same for both lock and unlock
        request->Set_completion_handler(
                comp_ctx,
                completion_handler);

        // processing handlers are different
        if (do_lock) {
            request->Set_processing_handler(
                    Make_callback(
                            &File_processor::Stream::Handle_lock,
                            this_ptr));

            ASSERT(!native_handle->cur_lock_request);
            // Cancellation handler is needed only for Lock.
            request->Set_cancellation_handler(
                    Make_callback(
                            &File_processor::Stream::Handle_lock_cancel,
                            this_ptr,
                            request));
            native_handle->cur_lock_request = request;
        } else {
            request->Set_processing_handler(
                    Make_callback(
                            &File_processor::Stream::Handle_unlock,
                            this_ptr));

            ASSERT(!native_handle->cur_unlock_request);
            native_handle->cur_unlock_request = request;
        }
        proc->Submit_request(request);
    }
    return request;
}

Operation_waiter
File_processor::Stream::Close_impl(Close_handler completion_handler,
                                   Request_completion_context::Ptr comp_ctx)
{
    std::unique_lock<std::mutex> lock(op_mutex);

    auto proc = processor.lock();
    if (!proc) {
        VSM_EXCEPTION(Exception, "Processor already destroyed");
    }

    auto this_ptr = Shared_from_this();

    Request::Ptr request = Request::Create();
    request->Set_completion_handler(comp_ctx, completion_handler);
    auto proc_handler =
        Make_callback(&File_processor::Stream::Handle_close, this_ptr, request);
    request->Set_processing_handler(proc_handler);
    request->Set_done_handler(proc_handler);
    proc->Submit_request(request);
    return request;
}

void
File_processor::Stream::Queue_write(Write_request::Ptr request)
{
    if (native_handle->cur_write_request) {
        write_queue.push_front(request);
        return;
    }
    auto proc = processor.lock();
    if (!proc) {
        VSM_EXCEPTION(Exception, "Processor already destroyed");
    }
    native_handle->cur_write_request = request;
    if (maintain_pos && request->Offset() == OFFSET_NONE) {
        request->Offset() = cur_pos;
    }
    proc->Submit_request(request);
}

void
File_processor::Stream::Push_write_queue()
{
    ASSERT(native_handle->cur_write_request);

    if (write_queue.empty()) {
        native_handle->cur_write_request = nullptr;
    } else {
        native_handle->cur_write_request = write_queue.back();
        write_queue.pop_back();
        if (maintain_pos &&
            native_handle->cur_write_request->Offset() == OFFSET_NONE) {

            native_handle->cur_write_request->Offset() = cur_pos;
        }
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }
        proc->Submit_request(native_handle->cur_write_request);
    }
}

void
File_processor::Stream::Queue_read(Read_request::Ptr request)
{
    if (native_handle->cur_read_request) {
        read_queue.push_front(request);
        return;
    }
    auto proc = processor.lock();
    if (!proc) {
        VSM_EXCEPTION(Exception, "Processor already destroyed");
    }
    native_handle->cur_read_request = request;
    if (maintain_pos && request->Offset() == OFFSET_NONE) {
        request->Offset() = cur_pos;
    }
    proc->Submit_request(request);
}

void
File_processor::Stream::Push_read_queue()
{
    ASSERT(native_handle->cur_read_request);

    if (read_queue.empty()) {
        native_handle->cur_read_request = nullptr;
    } else {
        native_handle->cur_read_request = read_queue.back();
        read_queue.pop_back();
        if (maintain_pos &&
            native_handle->cur_read_request->Offset() == OFFSET_NONE) {

            native_handle->cur_read_request->Offset() = cur_pos;
        }
        auto proc = processor.lock();
        if (!proc) {
            VSM_EXCEPTION(Exception, "Processor already destroyed");
        }
        proc->Submit_request(native_handle->cur_read_request);
    }
}

void
File_processor::Stream::Handle_write()
{
    std::unique_lock<std::mutex> lock(op_mutex);
    auto request_lock = native_handle->cur_write_request->Lock();
    if (native_handle->is_closed) {
        native_handle->cur_write_request->Set_result_arg(Io_result::CLOSED, request_lock);
        native_handle->cur_write_request->Complete(Request::Status::OK,
                                                   std::move(request_lock));
    } else if (native_handle->cur_write_request->Get_status() == Request::Status::CANCELING) {
        native_handle->cur_write_request->Set_result_arg(
                Io_result::CANCELED, request_lock);
        //XXX set size argument zero
        native_handle->cur_write_request->Complete(Request::Status::CANCELED,
                                                   std::move(request_lock));
        /* Queue will be pushed by completion handler. */
    } else {
        request_lock.unlock();
        native_handle->Write();
    }
}

void
File_processor::Stream::Handle_write_completion(Request::Handler completion_handler)
{
    std::unique_lock<std::mutex> lock(op_mutex);

    //XXX use real number of bytes written
    if (!native_handle->is_closed &&
        native_handle->cur_write_request->Get_last_result() == Io_result::OK &&
        maintain_pos) {

        cur_pos += native_handle->cur_write_request->Data_buffer()->Get_length();
    }

    /* Queue the next request if any. */
    Push_write_queue();

    /* Invoke user handler. */
    lock.unlock();
    completion_handler();
}

void
File_processor::Stream::Handle_read()
{
    std::unique_lock<std::mutex> lock(op_mutex);
    auto request_lock = native_handle->cur_read_request->Lock();

    if (native_handle->is_closed) {
        native_handle->cur_read_request->Set_result_arg(Io_result::CLOSED, request_lock);
        native_handle->cur_read_request->Set_buffer_arg(Io_buffer::Create(), request_lock);
        native_handle->cur_read_request->Complete(Request::Status::OK,
                                                  std::move(request_lock));
    } else if (native_handle->cur_read_request->Get_status() == Request::Status::CANCELING) {
        native_handle->cur_read_request->Set_result_arg(Io_result::CANCELED, request_lock);
        native_handle->cur_read_request->Set_buffer_arg(Io_buffer::Create(), request_lock);
        native_handle->cur_read_request->Complete(Request::Status::CANCELED,
                                                  std::move(request_lock));
        /* Queue will be pushed by completion handler. */
    } else {
        request_lock.unlock();
        native_handle->Read();
    }
}

void
File_processor::Stream::Handle_read_completion(Request::Handler completion_handler)
{
    std::unique_lock<std::mutex> lock(op_mutex);

    if (!native_handle->is_closed) {
        Io_buffer::Ptr read_buffer =
                native_handle->cur_read_request->Get_last_read_buffer();
        if (read_buffer && maintain_pos) {
            cur_pos += read_buffer->Get_length();
        }
    }

    /* Queue the next request if any. */
    Push_read_queue();

    /* Invoke user handler. */
    lock.unlock();
    completion_handler();
}

void
File_processor::Stream::Locker_thread()
{
    while (true) {
        std::unique_lock<std::mutex> local_lock(op_mutex);
        LLOG("=== Locker thread wakeup acq=%d, active=%d",
                lock_cb.flock_acquire_requested,
                lock_cb.flock_thread_active);

        bool file_locked = false;
        if (lock_cb.flock_acquire_requested) {
            local_lock.unlock();
            file_locked = native_handle->Lock();
            local_lock.lock();
            LLOG("=== Locker thread after lock: acq=%d, active=%d",
                    lock_cb.flock_acquire_requested,
                    lock_cb.flock_thread_active);
        }

        if (lock_cb.flock_acquire_requested) {
            // Returned from lock and there is still request pending.
            // Complete the request.
            if (file_locked) {
                Complete_request(native_handle->cur_lock_request, Io_result::OK);
                lock_cb.flock_acquired = true;
            } else {
                Complete_request(native_handle->cur_lock_request, Io_result::LOCK_ERROR);
            }
            lock_cb.flock_acquire_requested = false;
        } else {
            // Returned from lock but not interested in lock any more.
            // Unlock and continue.
            if (file_locked && !Is_closed()) {
                if (!native_handle->Unlock()) {
                    VSM_SYS_EXCEPTION("Failed to unlock file");
                }
            }
        }

        if (!lock_cb.flock_thread_active) {
            break;
        }

        lock_cb.flock_notifier.wait(
                local_lock,
                [&]()
                {
                    return      lock_cb.flock_acquire_requested
                            ||  !lock_cb.flock_thread_active;
                });
    }
}

void
File_processor::Stream::Handle_lock()
{
    if (    native_handle->cur_lock_request->Get_status() == Request::Status::CANCELING
        ||  Is_closed())
    {
        Complete_request(native_handle->cur_lock_request);
    } else {
        std::unique_lock<std::mutex> lock(op_mutex);

        if (    lock_cb.flock_acquire_requested
            ||  lock_cb.flock_acquired) {
            // Double lock.
            LLOG("=----- Lock double lock");
            Complete_request(native_handle->cur_lock_request, Io_result::LOCK_ERROR);
        } else {
            lock_cb.flock_acquire_requested = true;
            if (lock_cb.flock_thread_active) {
                lock_cb.flock_notifier.notify_all();
            } else {
                lock_cb.flock_thread_active = true;
                auto t = std::thread(
                        &File_processor::Stream::Locker_thread,
                        Shared_from_this());
                t.detach();
                /* The tread will hold the reference to Stream::Ptr to avoid
                 * destruction while the thread is still running.
                 */
            }
        }
    }
}

void
File_processor::Stream::Handle_unlock()
{
    if (native_handle->cur_unlock_request->Get_status() == Request::Status::CANCELING) {
        Complete_request(native_handle->cur_unlock_request);
    } else {
        std::unique_lock<std::mutex> lock(op_mutex);
        auto unlock_result = Io_result::OK;
        if (lock_cb.flock_acquire_requested) {
            lock_cb.flock_acquire_requested = false;
            Complete_request(native_handle->cur_lock_request, Io_result::LOCK_ERROR);
        } else if (lock_cb.flock_acquired) {
            if (native_handle->Unlock()) {
                lock_cb.flock_acquired = false;
            } else {
                LOG_ERROR("UnlockFile: %s", Log::Get_system_error().c_str());
                unlock_result = Io_result::LOCK_ERROR;
            }
        } else {
            unlock_result = Io_result::LOCK_ERROR;
        }
        Complete_request(native_handle->cur_unlock_request, unlock_result);
    }
}

void
File_processor::Stream::Complete_request(Io_request::Ptr& request, Io_result result)
{
    if (request) {
        auto request_lock = request->Lock();
        if (    request->Get_status() == Request::Status::CANCELING
            ||  request->Get_status() == Request::Status::PROCESSING)
        {
            if (Is_closed()) {
                result = Io_result::CLOSED;
            } else {
                // If request failed we can override error.
                // We should not do that if request has succeeded.
                if (result != Io_result::OK) {
                    if (request->Timed_out()) {
                        result = Io_result::TIMED_OUT;
                    } else if (request->Get_status() == Request::Status::CANCELING) {
                        result = Io_result::CANCELED;
                    }
                }
            }
            request->Set_result_arg(result, request_lock);
            request->Complete(Request::Status::OK, std::move(request_lock));
        }
        request = nullptr;
    }
}

void
File_processor::Stream::Handle_close(Request::Ptr request)
{
    std::unique_lock<std::mutex> lock(op_mutex);
    if (request && request->Is_completed()) {
        return;
    }

    if (!native_handle->is_closed) {
        native_handle->Close();
        state = State::CLOSED;
    }

    Cancel_lock_operation(true);

    Request::Ptr read_request = native_handle->cur_read_request;
    Request::Ptr write_request = native_handle->cur_write_request;

    lock.unlock();

    if (read_request) {
        read_request->Cancel();
    }
    if (write_request) {
        write_request->Cancel();
    }
    if (request) {
        request->Complete();
    }
}

Io_stream::Offset
File_processor::Stream::Seek(Offset pos, bool is_relative)
{
    if (!maintain_pos) {
        return 0;
    }

    std::unique_lock<std::mutex> lock(op_mutex);
    Offset new_pos = cur_pos;
    if (is_relative) {
        new_pos += pos;
    } else {
        new_pos = pos;
    }
    if (new_pos < 0) {
        VSM_EXCEPTION(Invalid_param_exception,
                      "Invalid position specified (negative)");
    }
    cur_pos = new_pos;
    return new_pos;
}

void
File_processor::Stream::Handle_write_cancel(Write_request::Ptr request)
{
    std::unique_lock<std::mutex> lock(op_mutex);
    if (request != native_handle->cur_write_request) {
        /* Too late. */
        return;
    }
    if (!native_handle->Cancel_write()) {
        /* Already in progress, cannot cancel. */
        return;
    }
    auto request_lock = request->Lock();
    native_handle->cur_write_request->Set_result_arg(
        state == State::CLOSED ? Io_result::CLOSED : Io_result::CANCELED,
        request_lock);
    //XXX set size argument zero
    native_handle->cur_write_request->Complete(Request::Status::CANCELED,
                                               std::move(request_lock));
}

void
File_processor::Stream::Handle_read_cancel(Read_request::Ptr request)
{
    std::unique_lock<std::mutex> lock(op_mutex);
    if (request != native_handle->cur_read_request) {
        /* Too late. */
        return;
    }
    if (!native_handle->Cancel_read()) {
        /* Already in progress, cannot cancel. */
        return;
    }
    auto request_lock = request->Lock();
    native_handle->cur_read_request->Set_result_arg(
        state == State::CLOSED ? Io_result::CLOSED : Io_result::CANCELED,
        request_lock);
    native_handle->cur_read_request->Set_buffer_arg(Io_buffer::Create(), request_lock);
    native_handle->cur_read_request->Complete(Request::Status::CANCELED,
                                              std::move(request_lock));
}

void
File_processor::Stream::Cancel_lock_operation(bool stop_locker_thread)
{
    if (native_handle->cur_lock_request) {
        // If acquire in progress, do complete the request.
        if (lock_cb.flock_acquire_requested) {
            lock_cb.flock_acquire_requested = false;
            LLOG("=----- cancel");
            Complete_request(native_handle->cur_lock_request, Io_result::CANCELED);
        }
    }

    if (lock_cb.flock_thread_active && stop_locker_thread) {
        lock_cb.flock_thread_active = false;
        lock_cb.flock_notifier.notify_all();
    }
}

void
File_processor::Stream::Handle_lock_cancel(Io_request::Ptr request)
{
    std::unique_lock<std::mutex> lock(op_mutex);
    if (request == native_handle->cur_lock_request) {
        Cancel_lock_operation(false);
    }
}

void
File_processor::Stream::Handle_write_abort()
{
    std::unique_lock<std::mutex> lock(op_mutex);
    Push_write_queue();
}

void
File_processor::Stream::Handle_read_abort()
{
    std::unique_lock<std::mutex> lock(op_mutex);
    Push_read_queue();
}

File_processor::Native_controller &
File_processor::Stream::Get_native_controller() const
{
    auto proc = processor.lock();
    ASSERT(proc);
    return *proc->native_controller.get();
}

/* ****************************************************************************/
/* File_processor::Stream::Native_handle class. */

void
File_processor::Stream::Native_handle::Set_stream(Stream::Ptr stream)
{
   this->stream = stream;
}

void
File_processor::Stream::Native_handle::Handle_write_abort()
{
    if (stream) {
        stream->Handle_write_abort();
    }
}

void
File_processor::Stream::Native_handle::Handle_read_abort()
{
    if (stream) {
        stream->Handle_read_abort();
    }
}

File_processor::Stream::Native_handle::Stream_ref_holder
File_processor::Stream::Native_handle::Set_write_activity(bool is_active,
    std::unique_lock<std::mutex> &&lock)
{
    if (is_active) {
        ASSERT(!is_closed);
        ASSERT(!write_active);
        write_active = stream;
        return Stream_ref_holder(stream, std::move(lock));
    }
    ASSERT(write_active);
    Stream_ref_holder stream_ref(write_active, std::move(lock));
    write_active = nullptr;
    return stream_ref;
}

File_processor::Stream::Native_handle::Stream_ref_holder
File_processor::Stream::Native_handle::Set_read_activity(bool is_active,
    std::unique_lock<std::mutex> &&lock)
{
    if (is_active) {
        ASSERT(!is_closed);
        ASSERT(!read_active);
        read_active = stream;
        return Stream_ref_holder(stream, std::move(lock));
    }
    ASSERT(read_active);
    Stream_ref_holder stream_ref(read_active, std::move(lock));
    read_active = nullptr;
    return stream_ref;
}

/* ****************************************************************************/
/* File_processor class. */

Singleton<File_processor> File_processor::singleton;

File_processor::File_processor():
    Request_processor("File processor"),
    native_controller(Native_controller::Create())
{
}

void
File_processor::On_enable()
{
    Request_processor::On_enable();
    native_controller->Enable();
    comp_ctx = Request_completion_context::Create(
            "File processor completion",
            Get_waiter());
    comp_ctx->Enable();
    worker = Request_worker::Create(
        "File processor worker",
        std::initializer_list<Request_container::Ptr>{Shared_from_this(), comp_ctx});
    worker->Enable();
}

void
File_processor::On_disable()
{
    native_controller->Disable();
    Set_disabled();
    worker->Disable();
    worker = nullptr;
    comp_ctx->Disable();
    comp_ctx = nullptr;
}

File_processor::Stream::Ref
File_processor::Open(const std::string &name, const std::string &mode, bool maintain_pos)
{
    auto stream = Stream::Create(Shared_from_this(), name, mode, maintain_pos);
    Register_stream(stream);
    return stream;
}

void
File_processor::Unregister_handle(Stream::Native_handle &handle)
{
    native_controller->Unregister_handle(handle);
}

void
File_processor::Register_stream(File_processor::Stream::Ptr stream)
{
    stream->native_handle->Set_stream(stream);
    native_controller->Register_handle(*stream->native_handle);
}
