// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/android_serial_processor.h>

using namespace ugcs::vsm;

Singleton<Android_serial_processor> Android_serial_processor::singleton;

JNIEXPORT void JNICALL
ugcs::vsm::Java_com_ugcs_vsm_Vsm_StreamReady(JNIEnv *env, jobject _this, jint streamId)
{
    Android_serial_processor::Get_instance()->Stream_ready(streamId);
}

JNIEXPORT void JNICALL
ugcs::vsm::Java_com_ugcs_vsm_Vsm_StreamClosed(JNIEnv *env, jobject _this, jint streamId)
{
    Android_serial_processor::Get_instance()->Stream_closed(streamId);
}

JNIEXPORT void JNICALL
ugcs::vsm::Java_com_ugcs_vsm_Vsm_StreamWriteComplete(JNIEnv *env, jobject _this,
                                                     jint streamId,
                                                     jboolean succeeded)
{
    Android_serial_processor::Get_instance()->On_write_complete(streamId, succeeded);
}

JNIEXPORT void JNICALL
ugcs::vsm::Java_com_ugcs_vsm_Vsm_StreamReadComplete(JNIEnv *env, jobject _this,
                                                    jint streamId, jobject buf)
{
    Io_buffer::Ptr io_buf;
    if (buf) {
        Java::Env env = Java::Get_env();
        jlong size = env->GetDirectBufferCapacity(buf);
        void *addr = env->GetDirectBufferAddress(buf);
        io_buf = Io_buffer::Create(addr, size);
    } else {
        io_buf = nullptr;
    }
    Android_serial_processor::Get_instance()->On_read_complete(streamId, io_buf);
}

Android_serial_processor::Stream::Stream(
    Android_serial_processor::Weak_ptr processor, const std::string &name,
    const Mode &mode):

    Io_stream(Type::ANDROID_SERIAL), processor(processor), mode(mode)
{
    Set_name(name);
    state = State::OPENING;
}

Android_serial_processor::Stream_entry::Stream_entry(
    Android_serial_processor::Ptr processor, const std::string &name,
    const Stream::Mode &mode, Open_request::Ptr open_req):

    stream(Stream::Create(processor, name, mode)), open_req(open_req)
{}

void
Android_serial_processor::Stream_entry::Complete_open(bool succeeded)
{
    ASSERT(!open_req->Is_completed());
    state = succeeded ? State::OPENED : State::FAILED;
    open_req->succeeded = succeeded;
    stream->Complete_open(succeeded);
    open_req->Complete();
    open_req = nullptr;
}

Android_serial_processor::Android_serial_processor():
    Request_processor("Android serial processor")
{}

Operation_waiter
Android_serial_processor::Open(const std::string &port_name,
                               const Stream::Mode &mode,
                               Open_handler completion_handler,
                               Request_completion_context::Ptr comp_ctx)
{
    Open_request::Ptr req = Open_request::Create();
    Stream_entry::Ptr stream = Stream_entry::Create(Shared_from_this(),
        port_name, mode, req);
    auto comp_handler =
        Make_callback(&Android_serial_processor::Handle_open_completion,
                      Shared_from_this(), req, stream, completion_handler);
    req->Set_completion_handler(comp_ctx, comp_handler);
    auto proc_handler =
        Make_callback(&Android_serial_processor::Handle_open, Shared_from_this(),
                      stream);
    req->Set_processing_handler(proc_handler);
    Submit_request(req);
    return req;
}

std::list<std::string>
Android_serial_processor::Enumerate_port_names()
{
    Java::Env env = Java::Get_env();
    jobject arrayObj = env.CallVsmMethod<jobject>("Cbk_EnumerateSerialPortNames",
                                                  "()[Ljava/lang/String;");
    Java::Array<jobject> res =
        env.GetArray<jobject>(arrayObj);
    auto list = std::list<std::string>();
    for (size_t i = 0; i < res.Size(); i++) {
        jobject s = res[i];
        list.push_back(env.GetString(s));
        env->DeleteLocalRef(s);
    }
    res.Release();
    return list;
}

void
Android_serial_processor::On_enable()
{
    worker = Request_worker::Create(
        "Android serial processor worker",
        std::initializer_list<Request_container::Ptr>{Shared_from_this()});
    Request_processor::On_enable();
    worker->Enable();
}

void
Android_serial_processor::On_disable()
{
    LOG_INFO("Android_serial_processor disabled");
    auto req = Request::Create();
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Process_on_disable,
                      Shared_from_this(), req));
    Submit_request(req);
    req->Wait_done(false);
    Set_disabled();
    worker->Disable();
}

void
Android_serial_processor::Process_on_disable(Request::Ptr req)
{
    /* Close all streams. */
    decltype(streams.begin()) it;
    while ((it = streams.begin()) != streams.end()) {
        Close_stream_impl(it->first);
    }
    Java::Detach_current_thread();
    req->Complete();
}

void
Android_serial_processor::Handle_open(Stream_entry::Ptr stream)
{
    Java::Env env = Java::Get_env();
    jstring name = env.WrapString(stream->stream->Get_name());
    const Stream::Mode &mode = stream->stream->Get_mode();
    stream->stream->id = env.CallVsmMethod<jint>("Cbk_CreateSerialStream",
        "(Ljava/lang/String;IIZZZ)I", name, mode.Get_baud(),
        mode.Get_char_size(), mode.Get_stop_bit(), mode.Get_parity_check(),
        mode.Get_parity());
    env->DeleteLocalRef(name);
    if (stream->stream->id < 0) {
        LOG_WARN("Java stream opening failed for '%s'",
                 stream->stream->Get_name().c_str());
        stream->state = Stream_entry::State::FAILED;
        stream->Complete_open(false);
    } else if (streams.find(stream->stream->id) != streams.end()) {
        LOG_ERROR("Duplicated stream ID on native side for '%s': %d",
                  stream->stream->Get_name().c_str(), stream->stream->id);
        stream->state = Stream_entry::State::FAILED;
        stream->Complete_open(false);
    } else {
        LOG_DEBUG("Java stream opening initiated for '%s': %d",
                  stream->stream->Get_name().c_str(), stream->stream->id);
        stream->state = Stream_entry::State::OPEN_REQUESTED;
        streams.emplace(stream->stream->id, stream);
    }
}

void
Android_serial_processor::Handle_open_completion(Open_request::Ptr req,
                                                 Stream_entry::Ptr stream,
                                                 Open_handler handler)
{
    handler(req->succeeded ?
        Io_stream::Ref(stream->stream) : Io_stream::Ref());
}

Operation_waiter
Android_serial_processor::Stream::Write_impl(
    Io_buffer::Ptr buffer, Offset, Write_handler completion_handler,
    Request_completion_context::Ptr comp_ctx)
{
    Android_serial_processor::Ptr proc = processor.lock();
    if (!proc) {
        return Operation_waiter();
    }
    Write_request::Ptr req =
        Write_request::Create(Shared_from_this(), buffer, completion_handler.Get_arg<0>());
    req->Set_completion_handler(comp_ctx, completion_handler);
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Handle_write, proc, req));
    proc->Submit_request(req);
    return req;
}

Operation_waiter
Android_serial_processor::Stream::Read_impl(
    size_t max_to_read, size_t min_to_read, Offset,
    Read_handler completion_handler, Request_completion_context::Ptr comp_ctx)
{
    Android_serial_processor::Ptr proc = processor.lock();
    if (!proc) {
        return Operation_waiter();
    }
    Read_request::Ptr req =
        Read_request::Create(Shared_from_this(), max_to_read, min_to_read,
                             completion_handler.Get_arg<0>(),
                             completion_handler.Get_arg<1>());
    req->Set_completion_handler(comp_ctx, completion_handler);
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Handle_read, proc, req));
    proc->Submit_request(req);
    return req;
}

Operation_waiter
Android_serial_processor::Stream::Close_impl(
    Close_handler completion_handler, Request_completion_context::Ptr comp_ctx)
{
    Android_serial_processor::Ptr proc = processor.lock();
    if (!proc) {
        return Operation_waiter();
    }
    Request::Ptr req = Request::Create();
    req->Set_completion_handler(comp_ctx, completion_handler);
    req->Set_processing_handler(Make_callback(
        &Android_serial_processor::Handle_stream_close, proc, req,
        Shared_from_this()));
    proc->Submit_request(req);
    return req;
}

void
Android_serial_processor::Stream_ready(int stream_id)
{
    LOG_DEBUG("Stream ready %d", stream_id);
    Request::Ptr req = Request::Create();
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Handle_stream_status,
                      Shared_from_this(), req, stream_id, true));
    Submit_request(req);
}

void
Android_serial_processor::Stream_closed(int stream_id)
{
    LOG_DEBUG("Stream closed %d", stream_id);
}

Android_serial_processor::Stream_entry::Ptr
Android_serial_processor::Find_stream(int stream_id)
{
    auto it = streams.find(stream_id);
    if (it == streams.end()) {
        LOG_WARN("Stream not found: %d", stream_id);
        return nullptr;
    }
    return it->second;
}

void
Android_serial_processor::Close_stream_impl(int stream_id)
{
    Stream_entry::Ptr stream = Find_stream(stream_id);
    if (!stream) {
        LOG_WARN("Stream not found: %d", stream_id);
        return;
    }
    ASSERT(stream->stream->id >= 0);
    stream->Close();
    Java::Env env = Java::Get_env();
    jboolean res = env.CallVsmMethod<jboolean>("Cbk_CloseSerialStream", "(I)Z",
                                               stream->stream->id);
    if (!res) {
        LOG_WARN("Failed to close stream on Java side");
    }
    streams.erase(stream->stream->id);
    stream->stream->Complete_close();
}

void
Android_serial_processor::Handle_stream_status(Request::Ptr req, int stream_id,
                                               bool succeeded)
{
    Stream_entry::Ptr stream = Find_stream(stream_id);
    if (stream) {
        if (!succeeded) {
            Close_stream_impl(stream_id);
        }
        stream->Complete_open(succeeded);
    } else {
        LOG_DEBUG("Stream not found for status report: %d", stream_id);
    }
    req->Complete();
}

void
Android_serial_processor::Handle_stream_close(Request::Ptr req, Stream::Ref stream)
{
    int id = stream->id;
    if (id >= 0) {
        Close_stream_impl(id);
    }
    req->Complete();
}

void
Android_serial_processor::Handle_write(Write_request::Ptr req)
{
    if (req->stream->id < 0) {
        req->Complete(Io_result::CLOSED);
        return;
    }
    Stream_entry::Ptr s = Find_stream(req->stream->id);
    if (!s) {
        req->Complete(Io_result::CLOSED);
        return;
    }
    s->Push_write(req);
}

void
Android_serial_processor::On_write_complete(int stream_id, bool succeeded)
{
    Request::Ptr req = Request::Create();
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Process_write_complete,
                      Shared_from_this(), req, stream_id, succeeded));
    Submit_request(req);
}

void
Android_serial_processor::Process_write_complete(Request::Ptr req, int stream_id,
                                                 bool succeeded)
{
    req->Complete();
    Stream_entry::Ptr s = Find_stream(stream_id);
    if (!s) {
        LOG_WARN("Stream not found for write completion: %d", stream_id);
        return;
    }
    s->Write_done(succeeded);
}

void
Android_serial_processor::Handle_read(Read_request::Ptr req)
{
    if (req->stream->id < 0) {
        req->Complete(Io_result::CLOSED);
        return;
    }
    Stream_entry::Ptr s = Find_stream(req->stream->id);
    if (!s) {
        req->Complete(Io_result::CLOSED);
        return;
    }
    s->Push_read(req);
}

void
Android_serial_processor::Stream_entry::Push_write(Write_request::Ptr req)
{
    if (req) {
        write_queue.push_back(req);
        if (write_queue.size() > 1) {
            /* Some request already in progress. */
            return;
        }
    }
    while (write_queue.size() > 0) {
        req = write_queue.front();
        Java::Env env = Java::Get_env();
        jobject buf = env->NewDirectByteBuffer(
            const_cast<void *>(req->buf->Get_data()), req->buf->Get_length());
        jboolean res = env.CallVsmMethod<jboolean>("Cbk_WriteSerialStream",
            "(ILjava/nio/ByteBuffer;)Z", stream->id, buf);
        env->DeleteLocalRef(buf);
        if (res) {
            return;
        }
        LOG_WARN("Write request failed");
        write_queue.pop_front();
        req->Complete(Io_result::OTHER_FAILURE);
    }
}

void
Android_serial_processor::Stream_entry::Write_done(bool succeeded)
{
    Write_request::Ptr req = write_queue.front();
    write_queue.pop_front();
    req->Complete(succeeded ? Io_result::OK : Io_result::OTHER_FAILURE);
    Push_write(nullptr);
}

void
Android_serial_processor::Stream_entry::Push_read(Read_request::Ptr req)
{
    if (req) {
        read_queue.push_back(req);
        if (read_queue.size() > 1) {
            /* Some request already in progress. */
            return;
        }
    }
    while (read_queue.size() > 0) {
        req = read_queue.front();
        Java::Env env = Java::Get_env();
        size_t read_size = req->max_to_read;
        if (req->buf_ref) {
            read_size -= req->buf_ref->Get_length();
        }
        jboolean res = env.CallVsmMethod<jboolean>("Cbk_ReadSerialStream",
            "(II)Z", stream->id, read_size);
        if (res) {
            return;
        }
        LOG_WARN("Read request failed");
        read_queue.pop_front();
        req->Complete(Io_result::OTHER_FAILURE);
    }
}

void
Android_serial_processor::On_read_complete(int stream_id, Io_buffer::Ptr buf)
{
    Request::Ptr req = Request::Create();
    req->Set_processing_handler(
        Make_callback(&Android_serial_processor::Process_read_complete,
                      Shared_from_this(), req, stream_id, buf));
    Submit_request(req);
}

void
Android_serial_processor::Process_read_complete(Request::Ptr req, int stream_id,
                                                Io_buffer::Ptr data)
{
    req->Complete();
    Stream_entry::Ptr s = Find_stream(stream_id);
    if (!s) {
        LOG_WARN("Stream not found for read completion: %d", stream_id);
        return;
    }
    s->Read_done(data);
}

void
Android_serial_processor::Stream_entry::Read_done(Io_buffer::Ptr data)
{
    Read_request::Ptr req = read_queue.front();
    if (!data) {
        req->Complete(Io_result::OTHER_FAILURE);
        Push_read(nullptr);
        return;
    }
    if (req->buf_ref) {
        req->buf_ref = req->buf_ref->Concatenate(data);
    } else {
        req->buf_ref = data;
    }
    if (req->buf_ref->Get_length() >= req->min_to_read) {
        read_queue.pop_front();
        req->Complete(Io_result::OK);
    }
    Push_read(nullptr);
}

void
Android_serial_processor::Stream_entry::Close()
{
    /* Complete all scheduled requests. */
    state = State::CLOSED;
    for (Write_request::Ptr req: write_queue) {
        req->Complete(Io_result::CLOSED);
    }
    write_queue.clear();
    for (Read_request::Ptr req: read_queue) {
        req->Complete(Io_result::CLOSED);
    }
    read_queue.clear();
    if (open_req) {
        open_req->succeeded = false;
        open_req->Complete();
        open_req = nullptr;
    }
}
