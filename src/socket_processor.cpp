// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of socket processor.
 */

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/param_setter.h>
#include <ugcs/vsm/utils.h>
#include <ugcs/vsm/debug.h>

#include <cstring>

using namespace ugcs::vsm;

namespace {

const int LISTEN_QUEUE_LEN = 5;

} /* anonymous namespace */

Singleton<Socket_processor> Socket_processor::singleton;

Operation_waiter
Socket_processor::Stream::Write_impl(Io_buffer::Ptr buffer,
                                     Offset offset,
                                     Write_handler completion_handler,
                                     Request_completion_context::Ptr comp_ctx)
{
    /* XXX processor should not depend on completion handler presence. */

    /* Use dummy completion handler, if none is provided. */
    if (!completion_handler) {
        completion_handler = Make_dummy_callback<void, Io_result>();
    }

    /* Use processor completion context, if none is provided. */
    if (!comp_ctx) {
        comp_ctx = processor->completion_ctx;
    }

    Write_request::Ptr request = Write_request::Create(buffer, Shared_from_this(),
                                                       offset,
                                                       completion_handler.template Get_arg<0>());
    request->Set_completion_handler(comp_ctx, completion_handler);
    auto proc_handler = Make_callback(&Socket_processor::On_write, processor, request, nullptr);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                                    processor, request));
    processor->Submit_request(request);
    return request;
}

Operation_waiter
Socket_processor::Stream::Read_impl(size_t max_to_read, size_t min_to_read,
                                    Offset offset,
                                    Read_handler completion_handler,
                                    Request_completion_context::Ptr comp_ctx)
{
    /* Use dummy completion handler, if none is provided. */
    if (!completion_handler) {
        completion_handler = Make_dummy_callback<void, Io_buffer::Ptr, Io_result>();
    }

    /* Use processor completion context, if none is provided. */
    if (!comp_ctx) {
        comp_ctx = processor->completion_ctx;
    }

    Read_request::Ptr request =
        Read_request::Create(completion_handler.template Get_arg<0>(),
                             max_to_read, min_to_read, Shared_from_this(), offset,
                             completion_handler.template Get_arg<1>());
    request->Set_completion_handler(comp_ctx, completion_handler);
    auto proc_handler = Make_callback(&Socket_processor::On_read, processor, request, nullptr);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
            processor, request));
    processor->Submit_request(request);
    return request;
}

Operation_waiter
Socket_processor::Stream::Read_from(
        size_t max_to_read,
        Read_from_handler completion_handler,
        Request_completion_context::Ptr comp_ctx)
{
    Read_request::Ptr request =
        Read_request::Create(completion_handler.Get_arg<0>(),
                             max_to_read,
                             1,
                             Shared_from_this(),
                             0,
                             completion_handler.Get_arg<1>());

    request->Set_completion_handler(comp_ctx, completion_handler);

    auto peer_addr = Socket_address::Create();
    completion_handler.Set_arg<2>(peer_addr);

    request->Set_processing_handler(
            Make_callback(&Socket_processor::On_read, processor, request, peer_addr));

    request->Set_cancellation_handler(
            Make_callback(&Socket_processor::Cancel_operation,
            processor, request));

    processor->Submit_request(request);
    return request;
}

Operation_waiter
Socket_processor::Stream::Write_to(
        Io_buffer::Ptr buffer,
        Socket_address::Ptr dest_addr,
        Write_handler completion_handler,
        Request_completion_context::Ptr comp_ctx)
{
    Write_request::Ptr request =
        Write_request::Create(buffer,
                             Shared_from_this(),
                             0,
                             completion_handler.Get_arg<0>());

    request->Set_completion_handler(comp_ctx, completion_handler);

    request->Set_processing_handler(
            Make_callback(&Socket_processor::On_write, processor, request, dest_addr));

    request->Set_cancellation_handler(
            Make_callback(&Socket_processor::Cancel_operation,
            processor, request));

    processor->Submit_request(request);
    return request;
}

Operation_waiter
Socket_processor::Stream::Close_impl(Close_handler completion_handler,
                                     Request_completion_context::Ptr comp_ctx)
{
    static Io_result unused_result_arg;

    Io_request::Ptr request = Io_request::Create(Shared_from_this(), OFFSET_NONE,
                                                 unused_result_arg);
    auto proc_handler = Make_callback(&Socket_processor::On_close, processor, request);
    request->Set_processing_handler(proc_handler);
    request->Set_completion_handler(comp_ctx, completion_handler);
    processor->Submit_request(request);
    return request;
}

Socket_address::Ptr
Socket_processor::Stream::Get_peer_address()
{
    return Socket_address::Create(peer_address);
}

Socket_address::Ptr
Socket_processor::Stream::Get_local_address()
{
    return Socket_address::Create(local_address);
}

bool
Socket_processor::Stream::Add_multicast_group(Socket_address::Ptr interface, Socket_address::Ptr multicast)
{
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(struct ip_mreq));
    mreq.imr_interface.s_addr = inet_addr(interface->Get_name_as_c_str());
    mreq.imr_multiaddr.s_addr = inet_addr(multicast->Get_name_as_c_str());
    auto ret = setsockopt(
            s,
            IPPROTO_IP, IP_ADD_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq),   // Win requires cast as it needs char* instead of void*
            sizeof(struct ip_mreq));
    if (ret) {
        LOG("Add_multicast_group failed: %s", Log::Get_system_error().c_str());
        return false;
    }
    return true;
}

bool
Socket_processor::Stream::Remove_multicast_group(Socket_address::Ptr interface, Socket_address::Ptr multicast)
{
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(struct ip_mreq));
    mreq.imr_interface.s_addr = inet_addr(interface->Get_name_as_c_str());
    mreq.imr_multiaddr.s_addr = inet_addr(multicast->Get_name_as_c_str());
    auto ret = setsockopt(
            s,
            IPPROTO_IP, IP_DROP_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq),   // Win requires cast as it needs char* instead of void*
            sizeof(struct ip_mreq));
    if (ret) {
        LOG("Remove_multicast_group failed: %s", Log::Get_system_error().c_str());
        return false;
    }
    return true;
}

bool
Socket_processor::Stream::Enable_broadcast(bool enable)
{
    int broadcast = enable?1:0;
    if (setsockopt(
            s,
            SOL_SOCKET,
            SO_BROADCAST,
            reinterpret_cast<const char*>(&broadcast),
            sizeof(broadcast))) {
        LOG("Enable_broadcast failed: %s", Log::Get_system_error().c_str());
        return false;
    }
    return true;
}

void
Socket_processor::Stream::Set_peer_address(Socket_address::Ptr addr)
{
    if (Get_type() != Io_stream::Type::UDP && Get_type() != Io_stream::Type::UDP_MULTICAST)
        return;
    static Io_result unused_result_arg;

    Io_request::Ptr request = Io_request::Create(Shared_from_this(), OFFSET_NONE,
                                                 unused_result_arg);
    request->Set_processing_handler(
            Make_callback(&Socket_processor::On_set_peer_address, processor, request, addr));
    processor->Submit_request(request);
}

Socket_processor::Socket_processor(Piped_request_waiter::Ptr piped_waiter) :
        Request_processor("Socket processor", piped_waiter),
        piped_waiter(piped_waiter)
{
    /* Ignore SIGPIPE signal. We get this error from select. */
//    Utils::Ignore_signal(SIGPIPE);
    sockets::Init_sockets();
}

Socket_processor::~Socket_processor()
{
    sockets::Done_sockets();
}

void
Socket_processor::Stream::Set_state(Io_stream::State state)
{
    this->state = state;
}

void
Socket_processor::Stream::Set_socket(sockets::Socket_handle s)
{
    ASSERT(this->s == INVALID_SOCKET);
    this->s = s;
}

void
Socket_processor::Stream::Update_name()
{
    std::string prefix;
    switch (Get_type()) {
    case Type::TCP:
        prefix = "tcp:";
        break;
    case Type::UDP:
        prefix = "udp:";
        break;
    case Type::UDP_MULTICAST:
        prefix = "udp_mcast:";
        break;
    case Type::CAN:
        prefix = "can:";
        break;
    default:
        prefix = "remote:";
        break;
    }
    Set_name(prefix + peer_address->Get_as_string());
}

void
Socket_processor::Stream::Set_connect_request(Io_request::Ptr request)
{
    ASSERT((!connect_request && request) || !request);
    connect_request = request;
}

sockets::Socket_handle
Socket_processor::Stream::Get_socket()
{
    return s;
}

Io_request::Ptr
Socket_processor::Stream::Get_connect_request()
{
    return connect_request;
}

Socket_processor::Stream::~Stream()
{
    Close_socket();
}

void
Socket_processor::Stream::Close_socket()
{
    if (s != INVALID_SOCKET) {
        if (parent_stream == nullptr) {
            if (sockets::Close_socket(s)) {
                LOG_ERR("close socket failure: %s", Log::Get_system_error().c_str());
            }
        }
        s = INVALID_SOCKET;
    }
    Set_state(Io_stream::State::CLOSED);
}

void
Socket_processor::On_write(Write_request::Ptr request, Socket_address::Ptr addr)
{
    Stream::Ptr stream = Lookup_stream(request->Get_stream());

    if (stream && stream->Get_state() != Io_stream::State::CLOSED)
    {
        stream->write_requests.emplace_back(Stream::Write_requests_entry(request, addr));
        Check_for_cancel_request(request, false);
    } else {
        request->Set_result_arg(Io_result::CLOSED);
        request->Complete();
    }
}

void
Socket_processor::On_read(Read_request::Ptr request, Socket_address::Ptr addr)
{
    Stream::Ptr stream = Lookup_stream(request->Get_stream());

    if (stream && stream->Get_state() != Io_stream::State::CLOSED)
    {
        stream->read_requests.emplace_back(Stream::Read_requests_entry(request, addr));
        Check_for_cancel_request(request, false);
        // Try to satisfy request from cache, first.
        if (stream->Get_type() == Stream::Type::UDP) {
            stream->Process_udp_read_requests();
        }
    } else {
        request->Set_result_arg(Io_result::CLOSED);
        request->Complete();
    }
}

void
Socket_processor::Stream::Abort_pending_requests(Io_result result)
{
    if (connect_request) {
        connect_request->Set_result_arg(result);
        connect_request->Complete();
        connect_request = nullptr;
    }

    for (auto &request : write_requests) {
        request.first->Set_result_arg(result);
        request.first->Complete();
    }
    write_requests.clear();

    for (auto &request : read_requests) {
        request.first->Set_result_arg(result);
        request.first->Complete();
    }
    read_requests.clear();

    for (auto &request : accept_requests) {
        request->Set_result_arg(result);
        request->Complete();
    }
    accept_requests.clear();
}

void
Socket_processor::Stream::Process_udp_read_requests()
{
    while (!read_requests.empty() && !packet_cache.Is_empty()) {
        // Satisfy the read request of substream.
        auto req = read_requests.front().first;
        auto locker = req->Lock();
        if (req->Is_processing()) {
            // request is still valid. satisfy it.
            Cache_entry data;
            if (packet_cache.Pull(data)) {
                auto readmax = req->Get_max_to_read();
                if (readmax < data.first->size()) {
                    data.first->resize(readmax);
                }
                req->Set_buffer_arg(
                        Io_buffer::Create(std::move(data.first)),
                        locker);
                req->Set_result_arg(Io_result::OK, locker);
                auto address_ptr = read_requests.front().second;
                if (address_ptr) {
                    *address_ptr = *data.second;
                }
                req->Complete(Request::Status::OK, std::move(locker));
                read_requests.pop_front();
            }
        } else if (req->Is_aborted()) {
            // Do not care about aborted requests.
            read_requests.pop_front();
        } else {
            // Cancelled requests are handled in On_cancel()
            // Let On_cancel handle the possibly cancelled request and then get back here for other pending requests.
            break;
        }
    }
}

void
Socket_processor::On_enable()
{
    Request_processor::On_enable();
    completion_ctx = Request_completion_context::Create(
            "Socket processor completion",
            piped_waiter);
    completion_ctx->Enable();
    thread = std::thread(&Socket_processor::Processing_loop, Shared_from_this());
}

void
Socket_processor::On_disable()
{
    auto req = Request::Create();
    req->Set_processing_handler(
            Make_callback(
                    &Socket_processor::Process_on_disable,
                    Shared_from_this(),
                    req));
    Submit_request(req);
    req->Wait_done(false);
    Set_disabled();
    /* Wait for worker thread terminates. */
    thread.join();
    completion_ctx->Disable();
    completion_ctx = nullptr;
}

void
Socket_processor::Process_on_disable(Request::Ptr request)
{
    if (!streams.empty()) {
        LOG_ERROR("%zu streams are still present during socket processor disabling.",
                streams.size());
        ASSERT(false);
    }
    for (auto& stream : streams) {
        if (stream.second)
            stream.second->Abort_pending_requests();
    }
    streams.clear();
    request->Complete();
}

void
Socket_processor::On_wait_and_process()
{
    fd_set rfds, wfds, efds;
    sockets::Socket_handle max_handle;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    sockets::Socket_handle wait_pipe = piped_waiter->Get_wait_pipe();
    FD_SET(wait_pipe, &rfds);
    max_handle = wait_pipe;

    for (auto &stream_iter : streams) {
        Stream::Ptr &stream = stream_iter.second;
        if (stream)
        {
            sockets::Socket_handle s = stream->Get_socket();
            auto is_set = false;
            switch (stream->Get_state()) {
            case Io_stream::State::OPENING:
                FD_SET(s, &wfds);
                FD_SET(s, &efds);
                is_set = true;
                break;
            case Io_stream::State::OPENED:
                if (!stream->write_requests.empty()) {
                    FD_SET(s, &wfds);
                    is_set = true;
                }
                if (!stream->read_requests.empty()) {
                    FD_SET(s, &rfds);
                    is_set = true;
                }
                if (!stream->accept_requests.empty())
                {
                    FD_SET(s, &rfds);
                    is_set = true;
                }
                break;
            default:
                break;
            }
            if (is_set && s > max_handle) {
                max_handle = s;
            }
        }
    }

    int rc = select(max_handle + 1, &rfds, &wfds, &efds, nullptr);

    if (rc <= 0) {
        VSM_SYS_EXCEPTION("Socket_processor select error");
    }
    if (FD_ISSET(wait_pipe, &rfds)) {
        piped_waiter->Ack();
        Process_requests();
        completion_ctx->Process_requests();
        rc--;
    }

    for (auto stream_iter = streams.begin(); stream_iter != streams.end() && rc; ) {
        Stream::Ptr stream = stream_iter->second;
        if (stream && stream->parent_stream == nullptr) {
            auto sock = stream->Get_socket();
            if (sock != INVALID_SOCKET) {
                if (FD_ISSET(sock, &wfds)) {
                    rc--;
                    switch (stream->Get_state()) {
                    case Io_stream::State::OPENING:
                        Handle_select_connect(stream);
                        /* Start also read/write requests immediately, if any. */
                        Handle_write_requests(stream);
                        if (stream->Get_type() == Io_stream::Type::UDP) {
                            Handle_udp_read_requests(stream);
                        } else {
                            Handle_read_requests(stream);
                        }
                        break;
                    case Io_stream::State::OPENED:
                        Handle_write_requests(stream);
                        break;
                    default:
                        ASSERT(false);
                        break;
                    }
                }

                if (FD_ISSET(sock, &rfds)) {
                    rc--;
                    switch (stream->Get_state()) {
                    case Io_stream::State::OPENED:
                        if (stream->Get_type() == Io_stream::Type::UDP) {
                            Handle_udp_read_requests(stream);
                        } else {
                            Handle_select_accept(stream);
                            Handle_read_requests(stream);
                        }
                        break;
                    default:
                        break;
                    }
                }

                if (FD_ISSET(sock, &efds)) {
                    rc--;
                    Handle_select_connect(stream);
                }
            }

            if (stream->Is_closed()) {
                stream_iter = streams.erase(stream_iter);
            } else {
                stream_iter++;
            }
        } else {
            stream_iter++;
        }
    }
}

void
Socket_processor::Handle_select_connect(Stream::Ptr stream)
{
    Io_request::Ptr request = stream->Get_connect_request();
    if (request == nullptr) {
        return; // connect request got cancelled before we got here.
    }
    stream->Set_connect_request(nullptr);
    int err;
    socklen_t err_len = sizeof(err);
    // reinterpret_cast is to make this platform independent. on windows getsockopt requires char* instead of void*
    if (getsockopt(stream->Get_socket(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &err_len)) {
        VSM_SYS_EXCEPTION("Getsockopt for connect failed");
    }
    auto locker = request->Lock();
    Io_result result;
    switch (err) {
    case 0:
        result = Io_result::OK;
        break;
    case ETIMEDOUT:
        result = Io_result::TIMED_OUT;
        break;
    case ECONNREFUSED:
        result = Io_result::CONNECTION_REFUSED;
        break;
    default:
        result = Io_result::OTHER_FAILURE;
        break;
    }

    request->Set_result_arg(result, locker);
    if (result == Io_result::OK && request->Is_processing())
    {
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getsockname(stream->Get_socket(), reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
            stream->local_address = Socket_address::Create(addr);
        }
        stream->Set_state(Io_stream::State::OPENED);
        stream->is_connected = true;
    }

    request->Complete(Request::Status::OK, std::move(locker));

    // This must be done after request->Complete to avoid request mutex double lock in Abort_pending_requests()
    if (result != Io_result::OK)
        Close_stream(stream, false);
}

void
Socket_processor::Handle_select_accept(Stream::Ptr listen_stream)
{
    if (listen_stream->Get_type() != Io_stream::Type::TCP) {
        return;
    }
    while (!listen_stream->accept_requests.empty()) {
        // process accept requests
        auto request = listen_stream->accept_requests.front();

        auto locker = request->Lock();
        auto stream = Lookup_stream(request->Get_stream());
        auto close_stream = false;

        // execute work (accept) under request lock to avoid complex handling
        // when accept succeeds but all listener requests are aborted.

        if (request->Is_processing())
        {
            if (stream)
            {
                sockaddr_storage peer_addr;
                socklen_t ss_len = sizeof(sockaddr_storage);
                sockets::Socket_handle s1;

                s1 = accept(listen_stream->Get_socket(), reinterpret_cast<sockaddr*>(&peer_addr), &ss_len);
                if (s1 == INVALID_SOCKET) {
                    // accept failed or pending
                    if (sockets::Is_last_operation_pending()) {
                        // got the signal from select but there are no connections pending any more!
                        // leave the request in the list and wait for next signal.
                        break;
                    }

                    LOG_INFO("socket accept failed: %s", Log::Get_system_error().c_str());
                    close_stream = true;
                    request->Set_result_arg(Io_result::OTHER_FAILURE, locker);
                } else {
                    // success, got new connection. stream and request are still valid.
                    if (sockets::Make_nonblocking(s1) != 0)
                    {/* Fatal here. */
                        sockets::Close_socket(s1);
                        VSM_SYS_EXCEPTION("Socket %d failed to set Nonblocking", s1);
                    }
                    ASSERT(stream->Get_state() == Io_stream::State::OPENING_PASSIVE);
                    stream->peer_address = Socket_address::Create(peer_addr);
                    stream->Update_name();
                    struct sockaddr_storage addr;
                    socklen_t addr_len = sizeof(addr);
                    if (getsockname(s1, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
                        stream->local_address = Socket_address::Create(addr);
                    }
                    stream->Set_socket(s1);
                    stream->Set_state(Io_stream::State::OPENED);
                    request->Set_result_arg(Io_result::OK, locker);
                }
            } else {
                // request has no stream.
                request->Set_result_arg(Io_result::CLOSED, locker);
            }
        } else {
            // request aborted or cancelled
            request->Set_result_arg(Io_result::CANCELED, locker);
        }

        request->Complete(Request::Status::OK, std::move(locker));
        listen_stream->accept_requests.pop_front();
        if (close_stream)
            Close_stream(stream, false);
        // try next pending request.
    }
}

void
Socket_processor::Handle_write_requests(Stream::Ptr stream)
{
    /* Try to process as much write operations as we can without blocking. */
    while (!stream->write_requests.empty()) {
        /* Last write request which is waiting */
        auto request = stream->write_requests.front().first;

        auto dest_address = stream->write_requests.front().second;
        if (!stream->is_connected && dest_address == nullptr) {
            // Use default address if destination not specified explicitly in request
            dest_address = stream->peer_address;
        }

        // Lock the request for reading so it cannot get aborted in the middle of operation
        auto locker = request->Lock();
        auto buffer = request->Data_buffer();
        auto close_stream = false;
        request->Set_result_arg(Io_result::OK, locker);
        if (request->Is_processing()) {
            // Request is still fine to process.
            do {
                ssize_t written;
                if (dest_address) {
                    written = sendto(
                            stream->Get_socket(),
                            reinterpret_cast<const char*>(buffer->Get_data()),
                            buffer->Get_length(),
                            sockets::SEND_FLAGS,
                            dest_address->Get_sockaddr_ref(),
                            dest_address->Get_len());
                } else {
                    written = send(
                            stream->Get_socket(),
                            reinterpret_cast<const char*>(buffer->Get_data()),
                            buffer->Get_length(),
                            sockets::SEND_FLAGS);
                }
                if (written > 0) {
                    // Success. Update Bytes_written and try again if there is more data. */
                    buffer = buffer->Slice(written);
                    stream->written_bytes += written;
                } else if (written == 0) {
                    // zero bytes written. no error
                    if (buffer->Get_length() == 0) {
                        break;  // this request wanted to write zero bytes. complete it.
                    } else {
                        /* Not really sure about written == 0 here. I assume that select will trigger writable
                         * event when socket becomes ready to accept new data to be written.
                         * Maybe some timeout is needed to avoid busy loop in this case...
                         */
                        LOG_WARN("send wrote 0 bytes!");
                        return; // will continue later.
                    }
                } else if (sockets::Is_last_operation_pending()) {
                    // write pending
                    return;     // will continue later.
                } else {
                    // socket error. assume no other operations can be performed.
                    LOG_DEBUG("socket send error=%s", Log::Get_system_error().c_str());
                    close_stream = true;
                    request->Set_result_arg(Io_result::CLOSED, locker);
                    break;
                }
            } while (buffer->Get_length());
        } else if (request->Is_aborted()) {
            // Do not care about aborted requests.
            stream->write_requests.pop_front();
        } else {
            // Cancelled requests are handled in On_cancel()
            // Let On_cancel handle the possibly cancelled request and then get back here for other pending requests.
            return;
        }

        request->Complete(Request::Status::OK, std::move(locker));
        stream->write_requests.pop_front();
        stream->written_bytes = 0;
        if (close_stream) {
            Close_stream(stream, false);
        }
        // try next request
    }
    // handle pending writes for all substreams.
    auto it = stream->substreams.begin();
    while (it != stream->substreams.end()) {
        if (it->second->Is_closed()) {
            it = stream->substreams.erase(it);
        } else {
            Handle_write_requests(it->second);
            ++it;
        }
    }
}

void
Socket_processor::Handle_read_requests(Stream::Ptr stream)
{
    /* Try to process as much read operations as we can without blocking. */
    while (!stream->read_requests.empty()) {
        /* Last read request which is waiting */
        auto request = stream->read_requests.front().first;
        auto address_ptr = stream->read_requests.front().second;

        // Lock the request for reading so it cannot get aborted in the middle of operation
        auto locker = request->Lock();
        auto readmin = request->Get_min_to_read();
        auto readmax = request->Get_max_to_read();
        if (request->Is_processing()) {
            // Request is still fine to process.
            request->Set_result_arg(Io_result::OK, locker);
            auto close_stream = false;
            if (!stream->reading_buffer) {
                /* Reserve space for future reads. Copy avoided. */
                stream->reading_buffer = std::make_unique<std::vector<uint8_t>>(readmax);
                stream->read_bytes = 0;
                if (readmax == 0) {
                    LOG_WARN("Zero size read requested");
                }
            }
            ASSERT(stream->reading_buffer);
            ASSERT(stream->reading_buffer->size() >= stream->read_bytes);

            do {
                ssize_t read_bytes;
                if (address_ptr) {
                    // user asked for peer address.
                    auto len = address_ptr->Get_len();
                    read_bytes = recvfrom(
                            stream->Get_socket(),
                            reinterpret_cast<char*>(&(*stream->reading_buffer)[stream->read_bytes]),
                            stream->reading_buffer->size() - stream->read_bytes,
                            0,
                            address_ptr->Get_sockaddr_ref(),
                            &len);
                    address_ptr->Set_resolved(read_bytes > 0);
                } else {
                    read_bytes = recv(
                            stream->Get_socket(),
                            reinterpret_cast<char*>(&(*stream->reading_buffer)[stream->read_bytes]),
                            stream->reading_buffer->size() - stream->read_bytes,
                            0);
                }
                if (read_bytes > 0) {
                    // got data, try again.
                    stream->read_bytes += read_bytes;
                } else if (read_bytes == 0) {
                    // zero read or other end closed. (half-closed connection)
                    if (stream->read_bytes < readmin) {
                        // readmin was not reached. Report the stream as closed
                        // but return the read data anyway.
                        LOG("Stream half-close: %s", stream->Get_name().c_str());
                        request->Set_result_arg(Io_result::CLOSED, locker);
                    }
                    // else we got at least the bytes we asked for, so it's not
                    // an error.

                    // Do not close the stream as it can possibly
                    // still be used for writing...
                    break;
                } else if (sockets::Is_last_operation_pending()) {
                    // read pending
                    if (stream->read_bytes < readmin) {
                        // min_to_read not reached yet, will finish later.
                        return;
                    } else {
                        // we have reached the minimum requested size with next read pending.
                        // Truncate the temp buffer and return data.
                        break;
                    }
                } else {
                    // Socket error. assume no other operation can be performed.
                    LOG("Socket read error for stream '%s': %s",
                        stream->Get_name().c_str(),
                        Log::Get_system_error().c_str());
                    request->Set_result_arg(Io_result::CLOSED, locker);
                    close_stream = true;
                    break;
                }
            /* Loop read while there is still space in buffer.
             * Do not loop for udp connections as it can read only
             * one packet at a time, and if there is no space in the buffer to
             * fit the whole packet the error occurs (windows) or excess data
             * is discarded (linux).
             */
            } while (stream->read_bytes < readmax && stream->Get_type() == Io_stream::Type::TCP);

            stream->reading_buffer->resize(stream->read_bytes);
            request->Set_buffer_arg(
                    Io_buffer::Create(std::move(stream->reading_buffer)),
                    locker);
            stream->reading_buffer = nullptr;

            request->Complete(Request::Status::OK, std::move(locker));
            stream->read_requests.pop_front();

            if (close_stream)
                Close_stream(stream, false);
            // try next request
        } else if (request->Is_aborted()) {
            // Do not care about aborted requests.
            stream->read_requests.pop_front();
        } else {
            // cancelled requests are handled in On_cancel()
            // Let On_cancel handle the possibly cancelled request and then get back here for other pending requests.
            return;
        }
    }
}

void
Socket_processor::Handle_udp_read_requests(Stream::Ptr stream)
{
    while (true) {
        auto address_ptr = Socket_address::Create();
        // TODO: make this value configurable
        ssize_t read_bytes = MIN_UDP_PAYLOAD_SIZE_TO_READ;
        auto len = address_ptr->Get_len();
        auto buffer = std::make_unique<std::vector<uint8_t>>(read_bytes);
        read_bytes = recvfrom(
                stream->Get_socket(),
                reinterpret_cast<char*>(buffer->data()),
                read_bytes,
                0,
                address_ptr->Get_sockaddr_ref(),
                &len);

        if (read_bytes > 0) {
            // Got data. Let's look which stream it belongs to...
            buffer->resize(read_bytes);
            address_ptr->Set_resolved(true);
            auto ss = stream->substreams.find(address_ptr);
            if (ss != stream->substreams.end() && ss->second->Is_closed()) {
                stream->substreams.erase(ss);
                ss = stream->substreams.end();
            }
            if (ss == stream->substreams.end()) {
                // New peer address. See if we have accepts waiting.
                if (stream->accept_requests.empty()) {
                    // No accepts pending. Go over pending reads.
                    stream->packet_cache.Push(Stream::Cache_entry{std::move(buffer), address_ptr});
                    stream->Process_udp_read_requests();
                } else {
                    // Satisfy pending accept request on master stream.
                    auto req = stream->accept_requests.front();
                    auto locker = req->Lock();
                    if (req->Is_processing()) {
                        auto substream = Lookup_stream(req->Get_stream());
                        if (substream) {
                            substream->peer_address = Socket_address::Create(address_ptr);
                            substream->peer_address->Set_resolved(true);
                            substream->local_address = Socket_address::Create(stream->Get_local_address());
                            substream->local_address->Set_resolved(true);
                            substream->Update_name();
                            substream->Set_socket(stream->Get_socket());
                            substream->Set_state(Io_stream::State::OPENED);
                            substream->parent_stream = stream;

                            // Insert new stream into substreams.
                            stream->substreams.emplace(address_ptr, substream);

                            // Save data for later read.
                            substream->packet_cache.Push(Stream::Cache_entry{std::move(buffer), address_ptr});

                            req->Set_result_arg(Io_result::OK, locker);
                        } else {
                            req->Set_result_arg(Io_result::CLOSED, locker);
                        }
                        req->Complete(Request::Status::OK, std::move(locker));
                        stream->accept_requests.pop_front();
                        // Try reading next packet.
                        continue;
                    } else {
                        // aborted/cancelled requests are handled in On_cancel()
                        // Let On_cancel handle the possibly cancelled request
                        // and then get back here for other pending requests.
                        return;
                    }
                }
            } else {
                // This is known substream.
                ss->second->packet_cache.Push(Stream::Cache_entry{std::move(buffer), address_ptr});
                ss->second->Process_udp_read_requests();
            }
        } else if (read_bytes == 0) {
            // zero read or other end closed. (half-closed connection)
            // Do not close the stream as it can possibly
            // still be used for writing...
            LOG("0 read");
            return;
        } else if (sockets::Is_last_operation_pending()) {
            // read pending. No more data for now.
            return;
        } else {
            // Socket error. assume no other operation can be performed.
            LOG("Socket read error for stream '%s': %s. Closing",
                stream->Get_name().c_str(),
                Log::Get_system_error().c_str());
            // Let the caller remove it streams.
            Close_stream(stream, false);
            break;
        }
    }
}

Operation_waiter
Socket_processor::Connect(
        Socket_address::Ptr addr,
        Connect_handler completion_handler,
        Request_completion_context::Ptr completion_context,
        Io_stream::Type sock_type,
        Socket_address::Ptr src_addr)
{
    Stream::Ptr stream = Stream::Create(Shared_from_this(), sock_type);
    stream->Set_state(Io_stream::State::OPENING);

    Io_request::Ptr request = Io_request::Create(stream, Io_stream::OFFSET_NONE, completion_handler.Get_arg<1>());
    completion_handler.Set_arg<0>(stream); /* Ensures stream existence for completion handler. */
    stream->Set_connect_request(request);
    // Copy the address from user to avoid uncontrolled modification.
    stream->peer_address = Socket_address::Create(addr);
    if (src_addr) {
        stream->local_address = Socket_address::Create(src_addr);
    }
    stream->Update_name();
    request->Set_completion_handler(completion_context, completion_handler);

    request->Set_processing_handler(
            Make_callback(
                    &Socket_processor::On_connect,
                    Shared_from_this(),
                    request, stream));

    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                      Shared_from_this(), request));

    Submit_request(request);

    return request;
}

void
Socket_processor::On_connect(Io_request::Ptr request, Stream::Ptr stream)
{
    if (Check_for_cancel_request(request, false)) {
        return;
    }
    addrinfo hints;
    addrinfo *result = nullptr, *rp;
    sockets::Socket_handle s = INVALID_SOCKET;

    memset(&hints, 0, sizeof(addrinfo));
    switch (stream->Get_type()) {
    case Io_stream::Type::TCP:
        hints.ai_socktype = SOCK_STREAM;
        break;
    case Io_stream::Type::UDP:
    case Io_stream::Type::UDP_MULTICAST:
        hints.ai_socktype = SOCK_DGRAM;
        break;
    default:
        request->Set_result_arg(Io_result::BAD_ADDRESS);
        stream->Set_connect_request(nullptr);
        request->Complete();
        return;
    }
    hints.ai_family = AF_INET; /* Only IPv4 for now. */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    int rc = getaddrinfo(
            stream->peer_address->Get_name_as_c_str(),
            stream->peer_address->Get_service_as_c_str(),
            &hints,
            &result);
    if (rc) {
        LOG_INFO("getaddrinfo failed for [%s:%s]: %s",
                stream->peer_address->Get_name_as_c_str(),
                stream->peer_address->Get_service_as_c_str(),
                gai_strerror(rc));
        request->Set_result_arg(Io_result::BAD_ADDRESS);
        stream->Set_connect_request(nullptr);
        request->Complete();
    } else {
        for (rp = result; rp ; rp = rp->ai_next) {
            s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (s == INVALID_SOCKET) {
                LOG_INFO("socket creation failed: %s", Log::Get_system_error().c_str());
                continue;
            }
            // Try to bind to user specified local address.
            if (stream->local_address) {
                /**
                 * Portability crap. MacOS bind is broken. It does not accept 128 here
                 * which is the length of struct sockaddr.
                 * Use sizeof(struct sockaddr_in) for ipv4 instead.
                 */
                socklen_t addr_size;
                if (result->ai_family == AF_INET) {
                    addr_size = sizeof(struct sockaddr_in);
                } else {
                    addr_size = stream->local_address->Get_len();
                }
                if (bind(s,
                        stream->local_address->Get_sockaddr_ref(),
                        addr_size) != 0) {
                    LOG_INFO("Bind to %s failed: %s",
                            stream->local_address->Get_as_string().c_str(),
                            Log::Get_system_error().c_str());
                    sockets::Close_socket(s);
                    s = INVALID_SOCKET;
                    continue;
                }
            }

            if (sockets::Make_nonblocking(s) != 0)
            {/* Fatal here. */
                freeaddrinfo(result);
                sockets::Close_socket(s);
                VSM_SYS_EXCEPTION("Socket %d failed to set Nonblocking", s);
            }

            if (!connect(s, rp->ai_addr, rp->ai_addrlen)) {
                /* Shouldn't happen with non-blocking sockets though. */
                auto locker = request->Lock();
                if (request->Is_processing()) {
                    // normal operation
                    stream->Set_state(Io_stream::State::OPENED);
                    stream->Set_socket(s);
                    stream->is_connected = true;
                    request->Set_result_arg(Io_result::OK, locker);
                    // Add to our streams list.
                    streams[stream] = stream;
                } else {
                    // abort requested.
                    sockets::Close_socket(s);
                    stream->Set_state(Io_stream::State::CLOSED);
                    request->Set_result_arg(Io_result::CANCELED, locker);
                }
                stream->Set_connect_request(nullptr);
                request->Complete(Request::Status::OK, std::move(locker));
                break;
            }

            if (sockets::Is_last_operation_pending()) {
                /* Ok. Async connection in progress. */
                stream->Set_socket(s);
                // Add to our streams list.
                streams[stream] = stream;
                ASSERT(stream->Get_connect_request());
                break;
            }
            LOG_INFO("socket connect failure: %s", Log::Get_system_error().c_str());
            /* Try next resolved address. */
            sockets::Close_socket(s);
            s = INVALID_SOCKET;
        }
        if (s == INVALID_SOCKET) {
            LOG_INFO("All getaddrinfo results are unusable for connect.");
            request->Set_result_arg(Io_result::BAD_ADDRESS);
            stream->Set_connect_request(nullptr);
            request->Complete();
        }
    }
    if (result) {
        freeaddrinfo(result);
        result = nullptr;
    }
}

Operation_waiter
Socket_processor::Bind_can(
        std::string interface,
        std::vector<int> filter_messges,
        Listen_handler completion_handler,
        Request_completion_context::Ptr completion_context)
{
    Socket_listener::Ptr stream = Socket_listener::Create(Shared_from_this(), Io_stream::Type::CAN);
    completion_handler.Set_arg<0>(stream);
    Io_request::Ptr request = Io_request::Create(stream, Io_stream::OFFSET_NONE, completion_handler.Get_arg<1>());
    request->Set_completion_handler(completion_context, completion_handler);

    auto proc_handler = Make_callback(
            &Socket_processor::On_bind_can,
            Shared_from_this(),
            request,
            filter_messges,
            stream,
            interface);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                                    Shared_from_this(), request));
    Submit_request(request);
    return request;
}

Operation_waiter
Socket_processor::Listen(
        Socket_address::Ptr addr,
        Listen_handler completion_handler,
        Request_completion_context::Ptr completion_context,
        Io_stream::Type sock_type)
{
    Socket_listener::Ptr stream = Socket_listener::Create(Shared_from_this(), sock_type);
    completion_handler.Set_arg<0>(stream);
    Io_request::Ptr request = Io_request::Create(stream, Io_stream::OFFSET_NONE, completion_handler.Get_arg<1>());
    request->Set_completion_handler(completion_context, completion_handler);

    auto proc_handler = Make_callback(
            &Socket_processor::On_listen,
            Shared_from_this(),
            request,
            stream,
            addr);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                                    Shared_from_this(), request));
    Submit_request(request);
    return request;
}

void
Socket_processor::On_listen(Io_request::Ptr request, Stream::Ptr stream, Socket_address::Ptr addr)
{
    addrinfo hints;
    addrinfo *result = nullptr, *rp;
    sockets::Socket_handle s = INVALID_SOCKET;

    memset(&hints, 0, sizeof(addrinfo));
    switch (stream->Get_type()) {
    case Io_stream::Type::TCP:
        hints.ai_socktype = SOCK_STREAM;
        break;
    case Io_stream::Type::UDP:
    case Io_stream::Type::UDP_MULTICAST:
        hints.ai_socktype = SOCK_DGRAM;
        break;
    default:
        request->Set_result_arg(Io_result::BAD_ADDRESS);
        stream->Set_connect_request(nullptr);
        request->Complete();
        return;
    }
    hints.ai_family = AF_INET; /* Only IPv4 for now. */
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;   // require address to be numeric.
    hints.ai_protocol = 0; /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    int rc = getaddrinfo(
            addr->Get_name_as_c_str(),
            addr->Get_service_as_c_str(),
            &hints,
            &result);
    if (rc) {
        LOG_INFO("getaddrinfo failed for [%s]: %s",
                addr->Get_as_string().c_str(),
                gai_strerror(rc));
        request->Set_result_arg(Io_result::BAD_ADDRESS);
        request->Complete();
    } else {
        for (rp = result; rp ; rp = rp->ai_next) {
            s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (s == INVALID_SOCKET) {
                LOG_INFO("socket creation failed: %s", Log::Get_system_error().c_str());
                continue;
            }
            if (sockets::Make_nonblocking(s) != 0)
            {/* Fatal here. */
                freeaddrinfo(result);
                sockets::Close_socket(s);
                VSM_SYS_EXCEPTION("Socket %d failed to set Nonblocking", s);
            }
            if (sockets::Prepare_for_listen(s, stream->Get_type() == Io_stream::Type::UDP_MULTICAST) != 0) {
                // failed to set SO_REUSEADDR, but let's not die because of that
                LOG_ERR("Prepare_for_listen failed: %s", Log::Get_system_error().c_str());
            }
            if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0) {
                if (    (   stream->Get_type() == Io_stream::Type::TCP
                        &&  listen(s, LISTEN_QUEUE_LEN) == 0)
                    ||  stream->Get_type() == Io_stream::Type::UDP
                    ||  stream->Get_type() == Io_stream::Type::UDP_MULTICAST) {
                // success
                    auto locker = request->Lock();
                    if (request->Is_processing()) {
                        // request status is still OK
                        streams[stream] = stream;
                        stream->local_address = Socket_address::Create(addr);
                        stream->Set_name(stream->local_address->Get_as_string());
                        stream->Set_socket(s);
                        stream->Set_state(Io_stream::State::OPENED);
                        request->Set_result_arg(Io_result::OK, locker);
                    } else {
                        // cancel or abort requested.
                        sockets::Close_socket(s);
                        stream->Set_state(Io_stream::State::CLOSED);
                        request->Set_result_arg(Io_result::CANCELED, locker);
                    }
                    request->Complete(Request::Status::OK, std::move(locker));
                    break;
                } else {
                    LOG_INFO("Listen failed or invalid SOCK_TYPE: %s", Log::Get_system_error().c_str());
                }
            } else {
                LOG_INFO("Bind failed: %s", Log::Get_system_error().c_str());
            }

            /* Try next resolved address. */
            sockets::Close_socket(s);
            s = INVALID_SOCKET;
        }
        if (s == INVALID_SOCKET) {
            LOG_INFO("All getaddrinfo results for %s are unusable for bind.", addr->Get_as_string().c_str());
            request->Set_result_arg(Io_result::BAD_ADDRESS);
            request->Complete();
        }
    }
    if (result) {
        freeaddrinfo(result);
        result = nullptr;
    }
}

void
Socket_processor::Cancel_operation(Io_request::Ptr request_to_cancel)
{
    // Do not try to cancel aborted request.
    if (request_to_cancel->Is_processing())
    {
        Io_result result_arg;
        Io_request::Ptr request = Io_request::Create(nullptr, Io_stream::OFFSET_NONE, result_arg);
        request->Set_completion_handler(Request_temp_completion_context::Create(), Make_setter(result_arg));
        request->Set_processing_handler(Make_callback(&Socket_processor::On_cancel,
                                                      Shared_from_this(),
                                                      request, request_to_cancel));
        Submit_request(request);
        request->Wait_done();
    }
}

Operation_waiter
Socket_processor::Get_addr_info(
        const std::string& host,
        const std::string& service,
        addrinfo* hints,
        Get_addr_info_handler completion_handler,
        Request_completion_context::Ptr completion_context)
{
    completion_handler.Set_arg<0>(host);
    completion_handler.Set_arg<1>(service);
    Io_request::Ptr request = Io_request::Create(nullptr, Io_stream::OFFSET_NONE, completion_handler.Get_arg<3>());

    request->Set_completion_handler(completion_context, completion_handler);

    // use completion handler argument to send hints to implemetation
    // to avoid creating another variable for this.
    if (hints) {
        completion_handler.Set_arg<2>(std::list<addrinfo>{*hints});
    } else {
        // default hint
        completion_handler.Set_arg<2>(std::list<addrinfo>{{0, AF_UNSPEC, 0, 0, 0, nullptr, nullptr, nullptr}});
    }

    auto proc_handler = Make_callback(
            &Socket_processor::On_get_addr_info,
            Shared_from_this(),
            request,
            completion_handler);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                                    Shared_from_this(), request));
    Submit_request(request);
    return request;
}

void
Socket_processor::On_get_addr_info(Io_request::Ptr request, Get_addr_info_handler handler)
{
    request->Set_result_arg(Io_result::OTHER_FAILURE);
    auto host = handler.Get_arg<0>();
    auto service = handler.Get_arg<1>();
    auto hint = handler.Get_arg<2>();
    addrinfo* result = nullptr;
    // 1) this is a blocking call
    // 2) assumes hint always contains one element here.
    int rc = getaddrinfo(host.c_str(), service.c_str(), &hint.front(), &result);
    if (rc) {
        handler.Set_arg<2>(std::list<addrinfo>());
        LOG_INFO("getaddrinfo failed for [%s:%s]: %s",
                host.c_str(), service.c_str(), gai_strerror(rc));
        request->Set_result_arg(Io_result::BAD_ADDRESS);
    } else {
        std::list<addrinfo> ret;
        for (auto rp = result; rp ; rp = rp->ai_next) {
            ret.emplace_back(*rp);
        }
        handler.Set_arg<2>(std::move(ret));
        request->Set_result_arg(Io_result::OK);
    }
    if (result) {
        freeaddrinfo(result);
        result = nullptr;
    }
    request->Complete();
}

Operation_waiter
Socket_processor::Accept_impl(
        Socket_listener::Ref listener,
        Request::Handler completion_handler,
        Request_completion_context::Ptr completion_context,
        Stream::Ref& stream_arg,
        Io_result& result_arg)
{
    auto type = listener->Get_type();
    ASSERT(type == Io_stream::Type::TCP || type == Io_stream::Type::UDP);
    Stream::Ptr stream = Stream::Create(Shared_from_this(), type);
    stream->Set_state(Io_stream::State::OPENING_PASSIVE);
    stream_arg = Stream::Ref(stream);
    Io_request::Ptr request = Io_request::Create(stream, Io_stream::OFFSET_NONE, result_arg);
    request->Set_completion_handler(completion_context, completion_handler);

    auto proc_handler = Make_callback(&Socket_processor::On_accept,
                                      Shared_from_this(), request, stream, listener);
    request->Set_processing_handler(proc_handler);
    request->Set_cancellation_handler(Make_callback(&Socket_processor::Cancel_operation,
                                                    Shared_from_this(), request));
    Submit_request(request);
    return request;
}

void
Socket_processor::On_accept(Io_request::Ptr request, Stream::Ptr stream, Socket_listener::Ref listen_stream)
{
    streams[stream] = stream;
    listen_stream->accept_requests.push_back(request);
    Check_for_cancel_request(request, false);
}

void
Socket_processor::On_close(Io_request::Ptr request)
{
    auto stream = Lookup_stream(request->Get_stream());
    if (stream)
    {
        if (stream->Get_state() == Io_stream::State::CLOSED) {
            LOG_WARN("Close for already closed stream.");
        }
        Close_stream(stream);
        request->Complete();
    } else {
        /*
         * It could happen by [somewhat bad] design. Don't spam here.
         * LOG_WARN("Close for unknown stream.");
         */
        request->Complete();
    }
}

void
Socket_processor::On_set_peer_address(Io_request::Ptr request, Socket_address::Ptr addr)
{
    auto stream = Lookup_stream(request->Get_stream());
    if (stream) {
        stream->peer_address = Socket_address::Create(addr);
        stream->Update_name();
    }
    request->Complete();
}

void
Socket_processor::Close_stream(Stream::Ptr stream, bool remove_from_streams)
{
    if (stream)
    {
        // Close all substreams.
        for (auto s : stream->substreams) {
            Close_stream(s.second);
        }
        stream->substreams.clear();
        stream->Close_socket();
        stream->Abort_pending_requests();
        stream->packet_cache.Clear();
        if (remove_from_streams) {
            streams.erase(stream);
        }
    }
}

void
Socket_processor::On_cancel(Io_request::Ptr cancel_request, Io_request::Ptr request)
{
    ASSERT(cancel_request != request);
    if (Check_for_cancel_request(request, true))
        cancel_request->Set_result_arg(Io_result::OK);
    else
        cancel_request->Set_result_arg(Io_result::OTHER_FAILURE);
    // cancelation_result == OK means cancel succeeded and completion handler will get called with Io_result::CANCELED
    cancel_request->Complete();
}

bool
Socket_processor::Check_for_cancel_request(Io_request::Ptr request, bool force_cancel)
{
    // prevent aborting request in the middle here.
    auto locker = request->Lock();
    if (    (request->Is_processing() && force_cancel)
        ||  request->Get_status() == Io_request::Status::CANCELING // request got canceled before entering processing
        )
    {
        auto result = Io_result::CANCELED;
        if (request->Timed_out())
            result = Io_result::TIMED_OUT;
        auto stream = Lookup_stream(request->Get_stream());
        if (stream) {
            // this is an io request. it can be connect, accept, write or read.
            // First check if this is a connect request.
            if (request == stream->Get_connect_request()) {
                // trying to cancel connect request.
                ASSERT(stream->Get_state() == Io_stream::State::OPENING);
                stream->Close_socket();
                stream->Set_connect_request(nullptr);
                request->Set_result_arg(result, locker);
                request->Complete(Request::Status::OK, std::move(locker));
                Close_stream(stream);   // Connect canceled, close and complete any pending operations...
                return true;
            }

            // Check for read request.
            if (!stream->read_requests.empty()) {
                auto iter = stream->read_requests.begin();
                auto first_request = (*iter).first;
                while (iter != stream->read_requests.end())
                {
                    auto stream_request = (*iter).first;
                    if (request == stream_request) {
                        // Found it here.
                        stream->read_requests.erase(iter);
                        if (request == first_request) {
                            // this is the current read request! return as much data as we have.
                            if (stream->reading_buffer)
                            {
                                stream->reading_buffer->resize(stream->read_bytes);
                                stream_request->Set_buffer_arg(
                                        Io_buffer::Create(std::move(stream->reading_buffer)),
                                        locker);
                                stream->reading_buffer = nullptr;
                            } else {
                                stream_request->Set_buffer_arg(
                                        Io_buffer::Create(),
                                        locker);
                            }
                        }
                        request->Set_result_arg(result, locker);
                        request->Complete(Request::Status::OK, std::move(locker));
                        return true;
                    }
                    iter++;
                }
            } // read requests.

            // Check for write request.
            if (!stream->write_requests.empty())
            {
                auto iter = stream->write_requests.begin();
                auto first_request = (*iter).first;
                while (iter != stream->write_requests.end()) {
                    // roll through write requests.
                    auto stream_request = (*iter).first;
                    if (request == stream_request) {
                        // Found it here.
                        auto close_stream = false;
                        stream->write_requests.erase(iter);
                        request->Set_result_arg(result, locker);
                        if (request == first_request) {
                            // this is the current write request!
                            if (stream->written_bytes) {
                            // Currently, there is no way to return written bytes, close socket.
                                close_stream = true;
                                request->Set_result_arg(Io_result::CLOSED, locker);
                            }
                        }
                        request->Complete(Request::Status::OK, std::move(locker));
                        if (close_stream)
                            Close_stream(stream);
                        return true;
                    }
                    iter++;
                }
            } // write requests.

            // Check for accept request.
            if (!stream->accept_requests.empty()) {
                auto iter = stream->accept_requests.begin();
                while (iter != stream->accept_requests.end()) {
                    // roll through accept requests.
                    auto stream_request = *iter;
                    if (request == stream_request) {
                        // Found it here.
                        stream->accept_requests.erase(iter);
                        request->Set_result_arg(result, locker);
                        request->Complete(Request::Status::OK, std::move(locker));
                        return true;
                    }
                    iter++;
                }
            } // accept requests.
        }

        // if we are here the request was not found in our system. probably closed before we got here.
        auto ctx = request->Get_completion_context(std::move(locker));
        LOG_DEBUG("Canceling unknown request in state %d, completion context "
                "[%s] stream %s.",
                static_cast<int>(request->Get_status()),
                ctx ? ctx->Get_name().c_str() : "empty",
                stream ? stream->Get_name().c_str() : "[already closed]");
    }
    return false;
}

Socket_processor::Stream::Ptr
Socket_processor::Lookup_stream(Io_stream::Ptr io_stream)
{
    Stream::Ptr stream;
    auto iter = streams.find(io_stream);
    if (iter != streams.end()) {
        stream = iter->second;
    }
    return stream;
}

Local_interface::Local_interface(const std::string& name)
:name(name)
{
}
