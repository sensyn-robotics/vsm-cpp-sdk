// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file socket_processor.h
 *
 * Socket processor. Used to create and handle socket-based I/O streams.
 */
#ifndef _SOCKET_PROCESSOR_H_
#define _SOCKET_PROCESSOR_H_

#include <ugcs/vsm/io_request.h>
#include <ugcs/vsm/piped_request_waiter.h>
#include <ugcs/vsm/singleton.h>
#include <ugcs/vsm/socket_address.h>

#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace ugcs {
namespace vsm {

/** Socket processor. */
class Socket_processor: public Request_processor
{
    DEFINE_COMMON_CLASS(Socket_processor, Request_container)
public:
    /**
     * Constructor.
     * @param piped_waiter Request waiter based on a pipe to multiplex socket
     * and request operations using select.
     */
    Socket_processor(Piped_request_waiter::Ptr piped_waiter = Piped_request_waiter::Create());

    virtual
    ~Socket_processor();

    /** Create processor instance. */
    static Ptr
    Create();

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
    	return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Socket specific stream. */
    class Stream: public Io_stream
    {
        DEFINE_COMMON_CLASS(Stream, Io_stream)
    public:
        template<typename... Args>
        Stream(Socket_processor::Ptr processor, Args&& ...args):
               Io_stream(std::forward<Args>(args)...),
               processor(processor)
        {}

        typedef Reference_guard<Stream::Ptr> Ref;

        ~Stream();

        void
        Set_state(Io_stream::State state);

        void
        Set_connect_request(Io_request::Ptr request);

        void
        Close_socket();

        Io_request::Ptr
        Get_connect_request();

        int
        Get_socket_type(){ return socket_type; }

        void
        Abort_pending_requests(Io_result result = Io_result::CLOSED);

        /** Default prototype for read operation completion handler. */
        typedef Callback_proxy<void, Io_buffer::Ptr, Io_result, Socket_address::Ptr> Read_from_handler;

        /** Used to read from udp socket. Wrapper for recvfrom() call
         * completion handler has source address as parameters.
         * It can be used to subsequent Write_to() calls.
         */
        virtual Operation_waiter
        Read_from(
                size_t max_to_read,
                Read_from_handler completion_handler,
                Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());

        /** Used to write to udp socket. Wrapper for sendto() call.
         */
        virtual Operation_waiter
        Write_to(
                Io_buffer::Ptr,
                Socket_address::Ptr dest_addr,
                Write_handler completion_handler,
                Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create());

        /* Returns a newly created Socket_address::Ptr with peer address. */
        Socket_address::Ptr
        Get_peer_address();

        /* Set peer address for udp stream
         * All subsequent Write() calls will go to this address.*/
        void
        Set_peer_address(Socket_address::Ptr);

        /* Returns a newly created Socket_address::Ptr with local address. */
        Socket_address::Ptr
        Get_local_address();

    private:

        Socket_address::Ptr peer_address = nullptr;

        Socket_address::Ptr local_address = nullptr;

        sockets::Socket_handle s = INVALID_SOCKET;

        Socket_processor::Ptr processor;

        Io_request::Ptr connect_request;

        int socket_type;    // SOCK_DGRAM | SOCK_STREAM

        // true if socket was connected using connect() call.
        // I.e. no need to specify destination when doing send().
        // Applies to both SOCK_DGRAM and SOCK_STREAM
        bool is_connected = false;

        typedef std::pair<Write_request::Ptr, Socket_address::Ptr> Write_requests_entry;
        std::list<Write_requests_entry> write_requests;

        typedef std::pair<Read_request::Ptr, Socket_address::Ptr> Read_requests_entry;
        std::list<Read_requests_entry> read_requests;

        std::list<Io_request::Ptr> accept_requests;

        std::shared_ptr<std::vector<uint8_t>> reading_buffer;
        size_t read_bytes = 0;      // bytes read by current read request
        size_t written_bytes = 0;   // bytes written by current write request

        friend class Socket_processor;

        sockets::Socket_handle
        Get_socket();

        void
        Set_socket_type(int type);

        void
        Set_socket(sockets::Socket_handle s);

        /** Update the name of the stream based on peer address. */
        void
        Update_name();

        /** @see Io_stream::Write_impl */
        virtual Operation_waiter
        Write_impl(Io_buffer::Ptr buffer,
                   Offset offset,
                   Write_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx);

        /** @see Io_stream::Read_impl */
        virtual Operation_waiter
        Read_impl(size_t max_to_read, size_t min_to_read, Offset offset,
                  Read_handler completion_handler,
                  Request_completion_context::Ptr comp_ctx) override;

        /** @see Io_stream::Close_impl */
        virtual Operation_waiter
        Close_impl(Close_handler completion_handler,
                   Request_completion_context::Ptr comp_ctx) override;

    };

    /** Stream type is used for listener socket type also. */
    typedef Stream Socket_listener;

    /** Callback used in Get_addr_info()
     */
    typedef Callback_proxy<
            void,                       // return void
            std::string,                // host name as passed to Get_addr_info() call
            std::string,                // service as passed to Get_addr_info() call
            std::list<addrinfo>,        // list of addrinfo returned from Get_addr_info()
            Io_result                   // result of operation: OK|TIMED_OUT|BAD_ADDRESS|CANCELLED
            > Get_addr_info_handler;

    /** Callback used in Listen()
     */
    typedef Callback_proxy<
            void,                   // return void
            Stream::Ref,   // stream
            Io_result               // result of operation: OK|TIMED_OUT|BAD_ADDRESS|CANCELLED
            > Listen_handler;

    /** Callback used in Connect() is the same as for listen handler.
     */
    typedef Listen_handler Connect_handler;

    Operation_waiter
    Connect(std::string host, std::string service,
            Connect_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create(),
            int sock_type = SOCK_STREAM,
            Socket_address::Ptr src_addr = nullptr)
    {
        return Connect(
                Socket_address::Create(host, service),
                completion_handler,
                completion_context,
                sock_type,
                src_addr);
    }

    Operation_waiter
    Connect(Socket_address::Ptr dest_addr,
            Connect_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create(),
            int sock_type = SOCK_STREAM,
            Socket_address::Ptr src_addr = nullptr);

    template <class Callback_ptr>
    Operation_waiter
    Accept(Socket_listener::Ref listener,
            Callback_ptr completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create())
    {
        Callback_check_type<Callback_ptr, void, Stream::Ref, Io_result>();

        return Accept_impl(listener, completion_handler, completion_context,
                completion_handler->template Get_arg<0>(),
                completion_handler->template Get_arg<1>());
    }

    /** Interface for nonblocking call to getaddrinfo().
     * @param host Hostname to resolve
     * @param service Service name to resolve. common port name or numerid port as string.
     * @param hints Passed as hints argument for getaddrinfo() call.
     */
    Operation_waiter
    Get_addr_info(
            const std::string& host,
            const std::string& service,
            addrinfo* hints,
            Get_addr_info_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create());

    Operation_waiter
    Listen(
            const std::string& host,
            const std::string& service,
            Listen_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create(),
            int sock_type = SOCK_STREAM)
    {
        return Listen(Socket_address::Create(host, service), completion_handler, completion_context, sock_type);
    }

    Operation_waiter
    Listen(
            Socket_address::Ptr addr,
            Listen_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create(),
            int sock_type = SOCK_STREAM);

    /** create local UDP endpoint socket and associate stream with it.
     * See Listen_handler for completion handler parameters.
     *
     * There is no such thing as listen() on udp socket.
     * But we make it possible to create a similar workflow as for tcp.
     * How it works:
     * udp socket is bound on given ip_address:service
     * User can issue Read, Read_from or Write_to on returned stream
     * For simple Write() user must first call Set_peer_address() on stream.
     *
     * @param addr local ip address/port for listener socket. Should be numeric.
     */
    Operation_waiter
    Bind_udp(
            Socket_address::Ptr addr,
            Listen_handler completion_handler,
            Request_completion_context::Ptr completion_context = Request_temp_completion_context::Create())
    {
        return Listen(addr, completion_handler, completion_context, SOCK_DGRAM);
    }

protected:

    /** Worker thread of socket processor. */
    std::thread thread;

    Piped_request_waiter::Ptr piped_waiter;

    /** Default completion context handled by processor thread is used if user
     * did not specify one.
     */
    Request_completion_context::Ptr completion_ctx;

    /** Handle processor enabling. */
    virtual void
    On_enable() override;

    /** Handle disable request. */
    virtual void
    On_disable() override;

    /** Process disable in processor context. */
    void
    Process_on_disable(Request::Ptr);

    virtual void
    On_wait_and_process() override;

    void
    On_write(Write_request::Ptr request, Socket_address::Ptr addr = nullptr);

    void
    On_read(Read_request::Ptr request, Socket_address::Ptr addr = nullptr);

    void
    On_close(Io_request::Ptr request);

    void
    On_set_peer_address(Io_request::Ptr request, Socket_address::Ptr addr);

    void
    On_connect(Io_request::Ptr request, Stream::Ptr stream);

    void
    On_listen(Io_request::Ptr request, Stream::Ptr stream, Socket_address::Ptr addr);

    void
    On_accept(Io_request::Ptr request, Stream::Ptr stream, Stream::Ref listenstream);

    void
    On_cancel(Io_request::Ptr request, Io_request::Ptr request_to_cancel);

    void
    On_get_addr_info(Io_request::Ptr request, Get_addr_info_handler handler);

    Operation_waiter
    Accept_impl(Socket_listener::Ref listener,
            Request::Handler completion_handler,
            Request_completion_context::Ptr completion_context,
            Stream::Ref& stream_arg, Io_result& result_arg);

private:
    /** Lock for I/O streams when accessed from worker threads
     * simultaneously. Use this only if there will be more than one socket_processor thread.
     */
    //    std::mutex streams_lock;

    typedef std::unordered_map<Io_stream::Ptr, Stream::Ptr> Streams_map;

    // keep all open udp streams based on "peer_address:peer_port".
    typedef std::unordered_set<std::string> Udp_streams_map;

    Streams_map streams;

    /** Socket processor singleton instance. */
    static Singleton<Socket_processor> singleton;

    Stream::Ptr Lookup_stream(Io_stream::Ptr io_stream);

    enum class RW_socket_result {
        DONE,
        PARTIAL,
        CLOSED
    };

    void
    Handle_select_accept(Stream::Ptr stream);

    void
    Handle_select_connect(Stream::Ptr stream);

    void
    Handle_write_requests(Stream::Ptr stream);

    void
    Handle_read_requests(Stream::Ptr stream);

    /** Close and remove from streams. must be called with all stream requests unlocked!*/
    void
    Close_stream(Stream::Ptr stream, bool remove_from_streams = true);

    void
    Cancel_operation(Io_request::Ptr request_to_cancel);

    /** return true if request was cancelled successfully */
    bool
    Check_for_cancel_request(Io_request::Ptr request, bool force_cancel);
};

// @{
/** Convenience types aliases. */
typedef Socket_processor::Stream Socket_stream;
typedef Socket_processor::Socket_listener Socket_listener;
// @}

/** Convenience builder for socket connect operation callbacks. */
DEFINE_CALLBACK_BUILDER (
        Make_socket_connect_callback,
        (Socket_stream::Ref, Io_result),
        (nullptr, Io_result::OTHER_FAILURE))

/** Convenience builder for socket listen operation callbacks. */
DEFINE_CALLBACK_BUILDER (
        Make_socket_listen_callback,
        (Socket_listener::Ref, Io_result),
        (nullptr, Io_result::OTHER_FAILURE))

/** Convenience builder for socket accept operation callbacks. */
DEFINE_CALLBACK_BUILDER (
        Make_socket_accept_callback,
        (Socket_stream::Ref, Io_result),
        (nullptr, Io_result::OTHER_FAILURE))

/** Convenience builder for socket Read_from operation callbacks. */
DEFINE_CALLBACK_BUILDER (
        Make_socket_read_from_callback,
        (Io_buffer::Ptr, Io_result, Socket_address::Ptr),
        (nullptr, Io_result::OTHER_FAILURE, nullptr))

/** Convenience builder for Read operation callbacks. */
DEFINE_CALLBACK_BUILDER (
        Make_socket_read_callback,
        (Io_buffer::Ptr, Io_result),
        (nullptr, Io_result::OTHER_FAILURE))

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _SOCKET_PROCESSOR_H_ */
