// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * vehicle_detector.cpp
 *
 *  Created on: Jul 12, 2013
 *      Author: janis
 */

#include <ugcs/vsm/transport_detector.h>
#include <ugcs/vsm/serial_processor.h>
#include <ugcs/vsm/utils.h>

using namespace ugcs::vsm;

constexpr std::chrono::seconds Transport_detector::TCP_CONNECT_TIMEOUT;
constexpr size_t Transport_detector::ARBITER_NAME_MAX_LEN;
std::string Transport_detector::SERIAL_PORT_ARBITER_NAME_PREFIX("vsm-serial-port-arbiter-");

Singleton<Transport_detector> Transport_detector::singleton;

Transport_detector::Transport_detector() :
        Request_processor("Transport detector processor")
{
}

void
Transport_detector::Activate(bool activate)
{
    detector_active = activate;
}

void
Transport_detector::On_enable()
{
    Request_processor::On_enable();

    worker = Request_worker::Create(
        "Transport detector worker",
        std::initializer_list<Request_container::Ptr>{Shared_from_this()});
    worker->Enable();

    watchdog_timer = Timer_processor::Get_instance()->Create_timer(
            WATCHDOG_INTERVAL,
            Make_callback(&Transport_detector::On_timer, Shared_from_this()),
            worker);
}

void
Transport_detector::On_disable()
{
    auto req = Request::Create();
    req->Set_processing_handler(
            Make_callback(
                    &Transport_detector::Process_on_disable,
                    Shared_from_this(),
                    req));
    worker->Submit_request(req);
    req->Wait_done(false);
    Set_disabled();
    worker->Disable();
    worker = nullptr;
    watchdog_timer = nullptr;
}

void
Transport_detector::Process_on_disable(Request::Ptr request)
{
    watchdog_timer->Cancel();
    serial_detector_config.clear();
    active_config.clear();
    request->Complete();
}

void
Transport_detector::Add_detector(
        const std::string& prefix,
        Connect_handler handler,
        Request_processor::Ptr context,
        Properties::Ptr properties,
        char tokenizer)
{
    if (properties == nullptr) {
        properties = Properties::Get_instance();
    }

    int token_index_start = 0;

    // get the correct token index by counting tokenizers in prefix
    for (std::size_t pos = 0;
         pos != std::string::npos;
         pos = prefix.find_first_of(tokenizer, pos + 1), token_index_start++);

    // load serial port data
    for (auto it = properties->begin(prefix, tokenizer); it != properties->end(); it++)
    {
        int token_index = token_index_start;
        try
        {
            if (it[token_index] == "use_serial_arbiter") {
                auto use = properties->Get(*it);
                if (use == "yes") {
                    LOG("Enabling serial port arbiter");
                    use_serial_arbiter = true;
                } else if (use == "no") {
                    LOG("Disabling serial port arbiter");
                    use_serial_arbiter = false;
                } else {
                    LOG("Invalid 'use_serial_arbiter' value");
                }
            } else if (it[token_index] == "exclude") {
                Request::Ptr request = Request::Create();
                auto proc_handler = Make_callback(
                        &Transport_detector::Add_blacklisted_impl,
                        Shared_from_this(),
                        handler,
                        properties->Get(*it),
                        request);
                request->Set_processing_handler(proc_handler);
                Submit_request(request);
            } else {
                auto vpref = prefix + tokenizer + it[token_index];
                token_index++;
                if (it[token_index] == "name") {
                    auto name = properties->Get(*it);
                    for (auto baud_it = properties->begin(vpref, tokenizer); baud_it != properties->end(); baud_it++)
                    {
                        if (baud_it[token_index] == "baud") {
                            Request::Ptr request = Request::Create();
                            auto proc_handler = Make_callback(
                                    &Transport_detector::Add_serial_detector,
                                    Shared_from_this(),
                                    name,
                                    properties->Get_int(*baud_it),
                                    handler,
                                    context,
                                    request);
                            request->Set_processing_handler(proc_handler);
                            Submit_request(request);
                        }
                    }
                } else if (it[token_index] == "address") {
                    auto address = properties->Get(*it);
                    auto port = properties->Get(vpref + tokenizer + "tcp_port");
                    Request::Ptr request = Request::Create();
                    auto proc_handler = Make_callback(
                            &Transport_detector::Add_ip_detector,
                            Shared_from_this(),
                            Socket_address::Create(),               // empty local addr.
                            Socket_address::Create(address, port),
                            Port::Type::TCP_OUT,
                            handler,
                            context,
                            request);
                    request->Set_processing_handler(proc_handler);
                    Submit_request(request);
                } else if (it[token_index] == "udp_local_port") {
                    // This means udp listener is configured.
                    // It can have also local address to bind to and
                    // Remote address/port specified, too.
                    auto local_port = properties->Get(*it);
                    std::string local_address("0.0.0.0");
                    Socket_address::Ptr remote = nullptr;
                    if (properties->Exists(vpref + tokenizer + "udp_local_address")) {
                        local_address = properties->Get(vpref + tokenizer + "udp_local_address");
                    }
                    remote = Socket_address::Create(
                            properties->Get(vpref + tokenizer + "udp_address"),
                            properties->Get(vpref + tokenizer + "udp_port"));
                    Request::Ptr request = Request::Create();
                    auto proc_handler = Make_callback(
                            &Transport_detector::Add_ip_detector,
                            Shared_from_this(),
                            Socket_address::Create(local_address, local_port),
                            remote,
                            Port::Type::UDP_IN,
                            handler,
                            context,
                            request);
                    request->Set_processing_handler(proc_handler);
                    Submit_request(request);
                }
            }
        }
        catch (Exception&)
        {
            LOG_INFO("Error while reading property %s", (*it).c_str());
        }
    }
}

void
Transport_detector::Protocol_not_detected(Io_stream::Ref stream)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Transport_detector::Protocol_not_detected_impl,
            Shared_from_this(),
            stream,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

bool Transport_detector::Port_blacklisted(const std::string& port_name, Connect_handler handler)
{
    auto it = port_black_list.find(handler);
    if (it == port_black_list.end())
        return false;
    regex::smatch match;
    for (auto &re: it->second)
    {
        if (regex::regex_match(port_name, match, re))
            return true;
    }
    return false;
}

void
Transport_detector::Add_serial_detector(
        const std::string port_regexp,
        int baud, Connect_handler handler,
        Request_processor::Ptr ctx,
        Request::Ptr request)
{
    // put in specific set as active config will depend on existing ports on system.
    auto it = serial_detector_config.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(port_regexp),
            std::forward_as_tuple(port_regexp)).first;
    it->second.Add_detector(baud, handler, ctx);
    request->Complete();
}

void
Transport_detector::Add_ip_detector(
        Socket_address::Ptr local_addr,
        Socket_address::Ptr remote_addr,
        Port::Type type,
        Connect_handler handler,
        Request_processor::Ptr ctx,
        Request::Ptr request)
{
    // Put it directly in active config.
    std::string name;
    auto it = active_config.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(local_addr->Get_as_string() + "-" + remote_addr->Get_as_string()),
            std::forward_as_tuple(local_addr, remote_addr, type, worker)).first;
    it->second.Add_detector(0, handler, ctx);
    request->Complete();
}

void
Transport_detector::Add_blacklisted_impl(Connect_handler handler,
        const std::string port_regexp, Request::Ptr request)
{
    if (!port_regexp.empty()) {
        auto it = port_black_list.emplace(handler, std::list<regex::regex>()).first;
        it->second.emplace_back(port_regexp, platform_independent_filename_regex_matching_flag);
        LOG_INFO("Added blacklisted port='%s'", port_regexp.c_str());
    }
    request->Complete();
}

void
Transport_detector::Protocol_not_detected_impl(Io_stream::Ref stream,
        Request::Ptr request)
{
    for (auto &it : active_config) {
        auto &port = it.second;
        if (stream == port.Get_stream()) {
            port.Reopen_and_call_next_handler();
            break;
        }
    }
    request->Complete();
}

bool
Transport_detector::On_timer()
{
    // modify current config according to connected ports.
    auto detected_ports = Serial_processor::Enumerate_port_names();

    for (auto it = active_config.begin(); it != active_config.end();)
    {// Remove all serial ports which do not exist any more.
        if (it->second.Get_type() == Port::SERIAL)
        {
            auto found = false;
            for (auto& detected_port : detected_ports) {
                if (detected_port == it->first) {
                    found = true;
                    it++;
                    break;
                }
            }

            if (!found) {
                // this port is not usable any more. Remove from current config.
                LOG_INFO("Port '%s' removed", it->first.c_str());
                it = active_config.erase(it);
            }
        } else {
            it++;
        }
    }

    for (auto& detected_port : detected_ports)
    {// Add detectors for newly discovered serial ports.
        if (active_config.find(detected_port) == active_config.end())
        {// Newly detected port which is not in current config.
            auto new_port = false;
            for (auto &configured_port_entry : serial_detector_config)
            {// Match it against our configured detectors and add to list if needed.
                auto &configured_port = configured_port_entry.second;
                if (configured_port.Match_name(detected_port))
                {// port name matched one from config. Add detectors.
                    Shared_mutex_file::Ptr arbiter = nullptr;
                    auto emplace_result = active_config.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(detected_port),
                            std::forward_as_tuple(detected_port));
                    auto &current_port = emplace_result.first->second;
                    if (emplace_result.second && use_serial_arbiter) {
                        auto acquire_complete = Shared_mutex_file::Make_acquire_handler(
                                &Transport_detector::On_serial_acquired,
                                Shared_from_this(),
                                detected_port);
                        current_port.Create_arbiter(acquire_complete, worker);
                    }
                    for (auto &entry : configured_port.detectors) {
                        if (!Port_blacklisted(detected_port, entry.Get_handler()))
                        {// not in blacklist
                            current_port.Add_detector(entry.Get_baud(), entry.Get_handler(), entry.Get_ctx());
                            new_port = true;
                        }
                    }
                }
            }
            if (new_port)
                LOG_INFO("Port '%s' detected", detected_port.c_str());
        }
    }

    if (detector_active) {
        for (auto &entry: active_config) {
            entry.second.On_timer();
        }
    }

    return true;
}

Transport_detector::Port::Port(const std::string &name):
    state(NONE),
    name(name),
    current_detector(detectors.end()),
    stream(nullptr),
    re(name, platform_independent_filename_regex_matching_flag),
    worker(nullptr),
    type(SERIAL),
    arbiter(nullptr)
{
}

Transport_detector::Port::Port(Socket_address::Ptr local_addr, Socket_address::Ptr peer_addr, Type type, Request_worker::Ptr w):
    state(NONE),
    name(local_addr->Get_as_string()),
    local_addr(local_addr),
    peer_addr(peer_addr),
    current_detector(detectors.end()),
    stream(nullptr),
    re(name, platform_independent_filename_regex_matching_flag),
    worker(w),
    type(type)
{
}

Transport_detector::Port::~Port()
{
    socket_connecting_op.Abort();

    if (stream && !stream->Is_closed()) {
        stream->Close();
    }

    stream = nullptr;
    arbiter = nullptr;
}

void
Transport_detector::Port::On_timer()
{
    if (stream && stream->Is_closed())
    {// User has closed the stream.
        LOG("Port %s closed by user", stream->Get_name().c_str());
        stream = nullptr;
        if (arbiter) {
            arbiter->Release();
        }
        state = NONE;
    }

    if (state == NONE) {
        Reopen_and_call_next_handler(); // Start detection from scratch.
    }
}

void
Transport_detector::Port::Create_arbiter(Shared_mutex_file::Acquire_handler handler,  Request_worker::Ptr w)
{
    // Create shared data name by extracting only
    // alphanumeric chars form from serial port name.
    std::string my_name = name;
    my_name.erase(
            std::remove_if(
                    my_name.begin(),
                    my_name.end(),
                    [](char c){return !std::isalnum(c);}),
            my_name.end());
    my_name.insert(0, SERIAL_PORT_ARBITER_NAME_PREFIX);
    if (my_name.size() > ARBITER_NAME_MAX_LEN) {
        my_name.erase(ARBITER_NAME_MAX_LEN);
    }

    arbiter = Shared_mutex_file::Create(my_name);
    arbiter_callback = handler;
    worker = w;
}

void
Transport_detector::On_serial_acquired(Io_result res, const std::string& name)
{
    // First, we must find the port in active_config
    // because it could be removed by the time we are here.
    auto it = active_config.find(name);
    if (it != active_config.end()) {
        // With serial port arbitration we do not care if resource was
        // recovered or just-created. We got the lock and that is enough.
        it->second.Open_serial(res == Io_result::OK);
    } else {
        // port dissapeared. no need to do anything here.
        // it should be already released by Port destructor.
    }
}

void
Transport_detector::Port::Open_serial(bool ok_to_open)
{
    if (ok_to_open) {
        if (current_detector != detectors.end()) {
            auto baud = current_detector->Get_baud();
            try {
                auto mode = Serial_processor::Stream::Mode().Baud(baud);
                stream = Serial_processor::Get_instance()->Open(name, mode);
                LOG("Opened serial port %s", name.c_str());
                /* XXX Workaround against submitting into
                 * disabled context. Full redesign is needed to fix it properly.
                 */
                auto ctx_lock = current_detector->Get_ctx()->Get_waiter()->Lock_notify();
                if (current_detector->Get_ctx()->Is_enabled()) {
                    Request::Ptr request = Request::Create();
                    /* temporary handler to haul parameters to user context.
                     * We cannot call handler->Set_args() here directly because it is possible
                     * to overwrite other request which is on the way to get processed in
                     * user provided context (ctx)
                     * So, we pass our arguments along with the callback and let the user context
                     * call Set_args() just before invocation. This works assuming user context
                     * is executing in one thread only.
                     */
                    auto temp_handler = Make_callback(
                            [](std::string name, int baud, Io_stream::Ref stream,
                                    Connect_handler h, Request::Ptr req)
                                    {
                        h.Set_args(name, baud, nullptr, stream);
                        h.Invoke();
                        req->Complete();
                                    },
                                    name,
                                    baud,
                                    stream,
                                    current_detector->Get_handler(),
                                    request);
                    request->Set_processing_handler(temp_handler);
                    current_detector->Get_ctx()->Submit_request_locked(
                            request,
                            std::move(ctx_lock));
                } else {
                    /* Simulate jump to next detector. */
                    LOG_INFO("Workaround for disabled context [%s] in "
                            "transport detector used (serial)!",
                            current_detector->Get_ctx()->Get_name().c_str());
                    ctx_lock.Unlock();
                    stream->Close();
                }

                current_detector++;
                state = CONNECTED;
                return; // Success.
            } catch (const Exception &) {
                LOG("Open failed for serial port %s", name.c_str());
            }
        }
        // release the port if open failed.
        if (arbiter) {
            arbiter->Release();
        }
    }
    state = NONE;
}

void
Transport_detector::Port::Reopen_and_call_next_handler()
{
    if (stream) {
        // stream exists already. Reopen for next handler.
        if (!stream->Is_closed()) {
            stream->Close();
        }
        stream = nullptr;
        if (arbiter) {
            arbiter->Release();
        }
    }

    if (current_detector == detectors.end()) {
        // no more detectors specified for this port!
        // restart from beginning on next On_timer() call.
        current_detector = detectors.begin();
        state = NONE;
    } else {
        state = CONNECTING;

        switch (type)
        {
        case SERIAL:
            if (arbiter) {
                arbiter->Acquire(arbiter_callback, worker);
            } else {
                Open_serial(true);
            }
            break;
        case TCP_OUT:
            socket_connecting_op.Abort();
            socket_connecting_op =
                    Socket_processor::Get_instance()->Connect(
                    peer_addr,
                    Make_socket_connect_callback(
                            &Transport_detector::Port::Ip_connected,
                            this),
                    worker
                );
            socket_connecting_op.Timeout(Transport_detector::TCP_CONNECT_TIMEOUT);
            break;
        case UDP_IN:
            socket_connecting_op.Abort();
            socket_connecting_op =
                    Socket_processor::Get_instance()->Connect(
                    peer_addr,
                    Make_socket_listen_callback(
                            &Transport_detector::Port::Ip_connected,
                            this),
                    worker,
                    SOCK_DGRAM,
                    local_addr);

            break;
        default:
            LOG_ERR("Reopen_and_call_next_handler: unsupported port type %d", type);
            state = NONE;
            break;
        }
    }
}

void
Transport_detector::Port::Ip_connected(
        Socket_stream::Ref new_stream,
        Io_result res)
{
    if (res == Io_result::OK)
    {
        stream = new_stream;
        Request::Ptr request = Request::Create();
        std::string address;
        if (new_stream->Get_socket_type() == SOCK_STREAM) {
            address = peer_addr->Get_as_string();
        } else {
            new_stream->Set_peer_address(peer_addr);
            address = peer_addr->Get_as_string() + "->" + local_addr->Get_as_string();
        }
        auto handler = current_detector->Get_handler();
        auto ctx = current_detector->Get_ctx();
        /* XXX Workaround against submitting into
         * disabled context. Full redesign is needed to fix it properly.
         */
        auto ctx_lock = ctx->Get_waiter()->Lock_notify();
        if (ctx->Is_enabled()) {
            /* temporary handler to haul parameters to user context.
             * We cannot call handler->Set_args() here directly because it is possible
             * to overwrite other request which is on the way to get processed in
             * user provided context (ctx)
             * So, we pass our arguments along with the callback and let the user context
             * call Set_args() just before invocation. This works assuming user context
             * is executing in one thread only.
             */
            auto temp_handler = Make_callback(
                    [](std::string peer, Io_stream::Ref stream,
                    		Socket_address::Ptr peer_addr, Connect_handler h,
                            Request::Ptr request)
                            {
                h.Set_args(peer, 0, peer_addr, stream);
                h.Invoke();
                request->Complete();
                            },
                            address,
                            new_stream,
                            Socket_address::Create(peer_addr),
                            handler,
                            request);
            request->Set_processing_handler(temp_handler);
            ctx->Submit_request_locked(request, std::move(ctx_lock));
        } else {
            /* Simulate jump to next detector. */
            LOG_INFO("Workaround for disabled context [%s] in "
                    "transport detector used (socket)!",
                    ctx->Get_name().c_str());
            ctx_lock.Unlock();
            stream->Close();
        }
        state = CONNECTED;
        current_detector++;
    } else {
        state = NONE;
    }
}

Io_stream::Ref
Transport_detector::Port::Get_stream()
{
    return stream;
}

void
Transport_detector::Port::Add_detector(
        int baud,
        Connect_handler handler,
        Request_processor::Ptr ctx)
{
    // do naive search over the existing detector list to avoid duplicate detectors
    // when several configured regexps match the same real port name
    // could be done automatically if detectors was a set,
    // but there is no std implementation of set of tuples.

    auto duplicate_found = false;
    for (auto &it2 : detectors) {
        if (it2 == Detector_entry(baud, handler, ctx)) {
            duplicate_found = true;
            break;
        }
    }
    if (!duplicate_found) {
        detectors.emplace_back(baud, handler, ctx);
        /** Always put detectors at the end of list.
         * initialize the current_detector only when first element gets added. */
        if (detectors.size() == 1) {
            current_detector = detectors.begin();
        }
    }
}

bool
Transport_detector::Port::Match_name(std::string & name)
{
    regex::smatch match;
    return regex::regex_match(name, match, re);
}
