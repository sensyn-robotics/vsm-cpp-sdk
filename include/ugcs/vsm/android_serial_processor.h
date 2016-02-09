// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** @file android_serial_processor.h */

#ifndef ANDROID_SERIAL_PROCESSOR_H_
#define ANDROID_SERIAL_PROCESSOR_H_

#ifndef ANDROID
#error "This file should be included only for Android target"
#endif

#include <ugcs/vsm/serial_processor.h>
#include <ugcs/vsm/java.h>

namespace ugcs {
namespace vsm {

extern "C" {

JNIEXPORT void JNICALL
Java_com_ugcs_vsm_Vsm_StreamReady(JNIEnv *env, jobject _this, jint streamId);

JNIEXPORT void JNICALL
Java_com_ugcs_vsm_Vsm_StreamClosed(JNIEnv *env, jobject _this, jint streamId);

JNIEXPORT void JNICALL
Java_com_ugcs_vsm_Vsm_StreamWriteComplete(JNIEnv *env, jobject _this, jint streamId,
                                          jboolean succeeded);

JNIEXPORT void JNICALL
Java_com_ugcs_vsm_Vsm_StreamReadComplete(JNIEnv *env, jobject _this,
                                         jint streamId, jobject buf);

}

/** Working with serial ports on Android platform. It includes also Java-side
 * part it interacts with.
 */
class Android_serial_processor: public Request_processor {
    DEFINE_COMMON_CLASS(Android_serial_processor, Request_container)

public:

    Android_serial_processor();

    class Stream: public Io_stream {
        DEFINE_COMMON_CLASS(Stream, Io_stream)
    public:
        /** Reference type. */
        typedef Reference_guard<Ptr> Ref;

        using Mode = ugcs::vsm::Serial_processor::Stream::Mode;

        Stream(Android_serial_processor::Weak_ptr processor,
               const std::string &name, const Mode &mode);

        const Mode &
        Get_mode() const
        {
            return mode;
        }
    private:
        friend class Android_serial_processor;

        Android_serial_processor::Weak_ptr processor;
        /** Port communication mode settings. */
        Mode mode;
        /** Assigned ID, negative if not registered. */
        int id = -1;

        /** See @ref Io_stream::Write_impl() */
        virtual Operation_waiter
        Write_impl(Io_buffer::Ptr buffer, Offset offset,
                   Write_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx);

        /** See @ref Io_stream::Read_impl() */
        virtual Operation_waiter
        Read_impl(size_t max_to_read, size_t min_to_read, Offset offset,
                  Read_handler completion_handler,
                  Request_completion_context::Ptr comp_ctx);

        /** See @ref Io_stream::Close_impl() */
        virtual Operation_waiter
        Close_impl(Close_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx);

        void
        Complete_open(bool succeeded)
        {
            state = succeeded ? State::OPENED : State::CLOSED;
        }

        void
        Complete_close()
        {
            state = State::CLOSED;
            id = -1;
        }
    };

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Default prototype for open operation completion handler.
     * Stream argument is nun-null if succeeded, null otherwise.
     */
    typedef Callback_proxy<void, Io_stream::Ref> Open_handler;

    static std::list<std::string>
    Enumerate_port_names();

    /** Open serial device stream.
     *
     * @param port_name Name returned by Enumerate_port_names() method.
     * @param mode Port mode.
     * @return Operation waiter for open request.
     */
    Operation_waiter
    Open(const std::string &port_name, const Stream::Mode &mode = Stream::Mode(),
         Open_handler completion_handler = Make_dummy_callback<void, Stream::Ref>(),
         Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());

private:
    friend void Java_com_ugcs_vsm_Vsm_StreamReady(
        JNIEnv *env, jobject _this, jint streamId);
    friend void Java_com_ugcs_vsm_Vsm_StreamClosed(
        JNIEnv *env, jobject _this, jint streamId);
    friend void Java_com_ugcs_vsm_Vsm_StreamWriteComplete(
        JNIEnv *env, jobject _this, jint streamId, jboolean succeeded);
    friend void Java_com_ugcs_vsm_Vsm_StreamReadComplete(
        JNIEnv *env, jobject _this, jint streamId, jobject buf);

    class Open_request: public Request {
        DEFINE_COMMON_CLASS(Open_request, Request)
    public:
        using Request::Request;

        bool succeeded = false;
    };

    class Write_request: public Request {
        DEFINE_COMMON_CLASS(Write_request, Request)
    public:
        Stream::Ref stream;
        Io_buffer::Ptr buf;
        Io_result &result_ref;

        Write_request(Stream::Ref stream, Io_buffer::Ptr buf, Io_result &result_ref):
            stream(stream), buf(buf), result_ref(result_ref)
        {}

        void
        Complete(Io_result result)
        {
            result_ref = result;
            Request::Complete();
        }
    };

    class Read_request: public Request {
        DEFINE_COMMON_CLASS(Read_request, Request)
    public:
        Stream::Ref stream;
        size_t max_to_read, min_to_read;
        Io_result &result_ref;
        Io_buffer::Ptr &buf_ref;

        Read_request(Stream::Ref stream, size_t max_to_read, size_t min_to_read,
                     Io_buffer::Ptr &buf_ref, Io_result &result_ref):
            stream(stream),
            max_to_read(max_to_read), min_to_read(min_to_read),
            buf_ref(buf_ref), result_ref(result_ref)
        {}

        void
        Complete(Io_result result)
        {
            result_ref = result;
            Request::Complete();
        }
    };

    class Stream_entry {
        DEFINE_COMMON_CLASS(Stream_entry)
    public:
        enum class State {
            INITIAL,
            FAILED,
            /** Open requested from Java side. */
            OPEN_REQUESTED,
            OPENED,
            CLOSED
        };

        Stream::Ref stream;
        State state = State::INITIAL;
        Open_request::Ptr open_req;
        /** Front element is the current active request. */
        std::list<Write_request::Ptr> write_queue;
        /** Front element is the current active request. */
        std::list<Read_request::Ptr> read_queue;

        Stream_entry(Android_serial_processor::Ptr processor,
                     const std::string &name, const Stream::Mode &mode,
                     Open_request::Ptr open_req);

        void
        Complete_open(bool succeeded);

        void
        Push_write(Write_request::Ptr req);

        void
        Write_done(bool succeeded);

        void
        Push_read(Read_request::Ptr req);

        void
        Read_done(Io_buffer::Ptr data);

        void
        Close();
    };

    /** Singleton object. */
    static Singleton<Android_serial_processor> singleton;
    Request_worker::Ptr worker;
    /** Active streams. */
    std::map<int, Stream_entry::Ptr> streams;

    virtual void
    On_enable() override;

    virtual void
    On_disable() override;

    void
    Process_on_disable(Request::Ptr req);

    void
    Handle_open(Stream_entry::Ptr stream);

    void
    Handle_open_completion(Open_request::Ptr req, Stream_entry::Ptr stream,
                           Open_handler handler);

    void
    Stream_ready(int stream_id);

    void
    Stream_closed(int stream_id);

    Stream_entry::Ptr
    Find_stream(int stream_id);

    void
    Handle_stream_status(Request::Ptr req, int stream_id, bool succeeded);

    void
    Close_stream_impl(int stream_id);

    void
    Handle_stream_close(Request::Ptr req, Stream::Ref stream);

    void
    Handle_write(Write_request::Ptr req);

    void
    On_write_complete(int stream_id, bool succeeded);

    void
    Process_write_complete(Request::Ptr req, int stream_id, bool succeeded);

    void
    Process_read_complete(Request::Ptr req, int stream_id, Io_buffer::Ptr data);

    void
    Handle_read(Read_request::Ptr req);

    void
    On_read_complete(int stream_id, Io_buffer::Ptr buf);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* ANDROID_SERIAL_PROCESSOR_H_ */
