// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * vehicle_detector.cpp
 *
 *  Created on: Jul 12, 2013
 *      Author: janis
 */

#include <ugcs/vsm/transport_detector.h>
#include <ugcs/vsm/utils.h>

#ifdef ANDROID
#include <ugcs/vsm/android_serial_processor.h>
using Serial_processor_type = ugcs::vsm::Android_serial_processor;
#else
#include <ugcs/vsm/serial_processor.h>
using Serial_processor_type = ugcs::vsm::Serial_processor;
#endif

using namespace ugcs::vsm;

constexpr std::chrono::seconds Transport_detector::TCP_CONNECT_TIMEOUT;
constexpr std::chrono::seconds Transport_detector::PROXY_TIMEOUT;
constexpr size_t Transport_detector::ARBITER_NAME_MAX_LEN;
std::string Transport_detector::SERIAL_PORT_ARBITER_NAME_PREFIX("vsm-serial-port-arbiter-");

std::vector<uint8_t> Transport_detector::PROXY_SIGNATURE {0x56, 0x53, 0x4d, 0x50};

constexpr uint8_t Transport_detector::PROXY_COMMAND_HELLO;

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
#   ifdef ANDROID
    Java::Detach_current_thread();
#   endif
    request->Complete();
}

void
Transport_detector::Add_detector(
        Connect_handler handler,
        Request_processor::Ptr context,
        const std::string& prefix,
        Properties::Ptr properties,
        char tokenizer)
{
    if (properties == nullptr) {
        properties = Properties::Get_instance();
    }

    int count = 0;
    std::string::size_type start_pos = 0;
    while (std::string::npos != (start_pos = prefix.find(tokenizer, start_pos))) {
        ++start_pos;
        ++count;
    }

    const int POS_TYPE = count + 1;
    const int POS_ID = count + 2;
    const int POS_NAME = count + 3;

    // load port data
    for (auto it = properties->begin(prefix, tokenizer); it != properties->end(); it++)
    {
        try {
            if (it[POS_TYPE] == "local_listening_port") {
                auto port = properties->Get(*it);
                std::string local_address("0.0.0.0");
                if (properties->Exists(prefix + tokenizer + "local_listening_address")) {
                    local_address = properties->Get(prefix + tokenizer + "local_listening_address");
                }
                Add_ip_detector(
                    Socket_address::Create(local_address, port),
                    Socket_address::Create(),               // empty remote addr.
                    Port::Type::TCP_IN,
                    handler,
                    context);
                continue;
            } else if (it[POS_TYPE] == "local_listening_address") {
                continue;
            } else if (it[POS_TYPE] == "port") {
                auto port = properties->Get(*it);
                auto address = properties->Get(prefix + tokenizer + "address");
                int timeout = DEFAULT_RETRY_TIMEOUT;
                if (properties->Exists(prefix + tokenizer + "retry_timeout")) {
                    timeout = properties->Get_int(prefix + tokenizer + "retry_timeout");
                }
                Add_ip_detector(
                    Socket_address::Create(),               // empty local addr.
                    Socket_address::Create(address, port),
                    Port::Type::TCP_OUT,
                    handler,
                    context,
                    timeout);
                continue;
            } else if (it[POS_TYPE] == "address") {
                continue;
            } else if (it[POS_TYPE] == "retry_timeout") {
                continue;
            }
            auto vpref = prefix + tokenizer + it[POS_TYPE] + tokenizer + it[POS_ID];
            if (it[POS_TYPE] == "serial") {
                if (it[POS_ID] == "use_arbiter") {
                    auto use = properties->Get(*it);
                    if (use == "yes") {
                        LOG("Enabling serial port arbiter");
                        use_serial_arbiter = true;
                    } else if (use == "no") {
                        LOG("Disabling serial port arbiter");
                        use_serial_arbiter = false;
                    } else {
                        LOG("Invalid 'use_arbiter' value");
                    }
                } else if (it[POS_ID] == "exclude") {
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
                    if (it[POS_NAME] == "name") {
                        auto name = properties->Get(*it);
                        for (
                            auto baud_it = properties->begin(vpref, tokenizer);
                            baud_it != properties->end();
                            baud_it++)
                        {
                            if (baud_it[POS_NAME] == "baud") {
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
                    }
                }
            } else if (it[POS_TYPE] == "tcp_out" && it[POS_NAME] == "port") {
                auto port = properties->Get(vpref + tokenizer + "port");
                auto address = properties->Get(vpref + tokenizer + "address");
                int timeout = DEFAULT_RETRY_TIMEOUT;
                if (properties->Exists(prefix + tokenizer + "retry_timeout")) {
                    timeout = properties->Get_int(prefix + tokenizer + "retry_timeout");
                }
                Add_ip_detector(
                    Socket_address::Create(),               // empty local addr.
                    Socket_address::Create(address, port),
                    Port::Type::TCP_OUT,
                    handler,
                    context,
                    timeout);
            } else if (it[POS_TYPE] == "proxy" && it[POS_NAME] == "port") {
                auto port = properties->Get(vpref + tokenizer + "port");
                auto address = properties->Get(vpref + tokenizer + "address");
                int timeout = DEFAULT_RETRY_TIMEOUT;
                if (properties->Exists(prefix + tokenizer + "retry_timeout")) {
                    timeout = properties->Get_int(prefix + tokenizer + "retry_timeout");
                }
                Add_ip_detector(
                    Socket_address::Create(),               // empty local addr.
                    Socket_address::Create(address, port),
                    Port::Type::PROXY,
                    handler,
                    context,
                    timeout);
            } else if (it[POS_TYPE] == "tcp_in" && it[POS_NAME] == "local_port") {
                auto port = properties->Get(vpref + tokenizer + "local_port");
                std::string local_address("0.0.0.0");
                if (properties->Exists(vpref + tokenizer + "local_address")) {
                    local_address = properties->Get(vpref + tokenizer + "local_address");
                }
                Add_ip_detector(
                    Socket_address::Create(local_address, port),
                    Socket_address::Create(),               // empty remote addr.
                    Port::Type::TCP_IN,
                    handler,
                    context);
            } else if (it[POS_TYPE] == "can" && it[POS_NAME] == "name") {
                auto iface = properties->Get(vpref + tokenizer + "name");
                Request::Ptr request = Request::Create();
                auto proc_handler = Make_callback(
                    &Transport_detector::Add_file_detector,
                    Shared_from_this(),
                    iface,
                    Port::CAN,
                    handler,
                    context,
                    request);
                request->Set_processing_handler(proc_handler);
                Submit_request(request);
            } else if (it[POS_TYPE] == "pipe" && it[POS_NAME] == "name") {
                auto iface = properties->Get(vpref + tokenizer + "name");
                Request::Ptr request = Request::Create();
                auto proc_handler = Make_callback(
                    &Transport_detector::Add_file_detector,
                    Shared_from_this(),
                    iface,
                    Port::PIPE,
                    handler,
                    context,
                    request);
                request->Set_processing_handler(proc_handler);
                Submit_request(request);
            } else if (it[POS_TYPE] == "udp_in" && it[POS_NAME] == "local_port") {
                auto port = properties->Get(vpref + tokenizer + "local_port");
                std::string local_address("0.0.0.0");
                if (properties->Exists(vpref + tokenizer + "local_address")) {
                    local_address = properties->Get(vpref + tokenizer + "local_address");
                }
                Add_ip_detector(
                    Socket_address::Create(local_address, port),
                    Socket_address::Create(),
                    Port::Type::UDP_IN,
                    handler,
                    context);
            } else if (it[POS_TYPE] == "udp_any" && it[POS_NAME] == "local_port") {
                auto port = properties->Get(vpref + tokenizer + "local_port");
                std::string local_address("0.0.0.0");
                if (properties->Exists(vpref + tokenizer + "local_address")) {
                    local_address = properties->Get(vpref + tokenizer + "local_address");
                }
                Add_ip_detector(
                    Socket_address::Create(local_address, port),
                    Socket_address::Create(),
                    Port::Type::UDP_IN_ANY,
                    handler,
                    context);
            } else if (it[POS_TYPE] == "udp_out" && it[POS_NAME] == "address") {
                std::string local_port("0");
                std::string local_address("0.0.0.0");

                auto remote_address = properties->Get(vpref + tokenizer + "address");
                auto remote_port = properties->Get(vpref + tokenizer + "port");

                if (properties->Exists(vpref + tokenizer + "local_address")) {
                    local_address = properties->Get(vpref + tokenizer + "local_address");
                }

                if (properties->Exists(vpref + tokenizer + "local_port")) {
                    local_port = properties->Get(vpref + tokenizer + "local_port");
                }

                Add_ip_detector(
                    Socket_address::Create(local_address, local_port),
                    Socket_address::Create(remote_address, remote_port),
                    Port::Type::UDP_OUT,
                    handler,
                    context);
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
    for (auto &re : it->second)
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
            std::forward_as_tuple(port_regexp, Port::SERIAL)).first;
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
        int retry_timeout)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
        &Transport_detector::Add_ip_detector_impl,
        Shared_from_this(),
        local_addr,
        remote_addr,
        type,
        handler,
        ctx,
        request,
        retry_timeout);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

void
Transport_detector::Add_ip_detector_impl(
    Socket_address::Ptr local_addr,
    Socket_address::Ptr remote_addr,
    Port::Type type,
    Connect_handler handler,
    Request_processor::Ptr ctx,
    Request::Ptr request,
    int retry_timeout)
{
    // Put it directly in active config.
    auto it = active_config.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(local_addr->Get_as_string() + "-" + remote_addr->Get_as_string()),
            std::forward_as_tuple(local_addr, remote_addr, type, worker, retry_timeout)).first;
    it->second.Add_detector(0, handler, ctx);
    request->Complete();
}

void
Transport_detector::Add_file_detector(
    const std::string can_interface,
    Port::Type type,
    Connect_handler handler,
    Request_processor::Ptr ctx,
    Request::Ptr request)
{
    // Put it directly in active config.
    std::string name;
    auto it = active_config.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(can_interface),
            std::forward_as_tuple(can_interface, type, worker)).first;
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
    if (stream) {
        for (auto &it : active_config) {
            it.second.Protocol_not_detected(stream);
        }
    }
    request->Complete();
}

bool
Transport_detector::On_timer()
{
    // modify current config according to connected ports.
    auto detected_ports = Serial_processor_type::Enumerate_port_names();

    for (auto it = active_config.begin(); it != active_config.end();) {
        // Remove all serial ports which do not exist any more.
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

    for (auto& detected_port : detected_ports) {
        // Add detectors for newly discovered serial ports.
        if (active_config.find(detected_port) == active_config.end()) {
            // Newly detected port which is not in current config.
            auto new_port = false;
            for (auto &configured_port_entry : serial_detector_config) {
                // Match it against our configured detectors and add to list if needed.
                auto &configured_port = configured_port_entry.second;
                if (configured_port.Match_name(detected_port)) {
                    // port name matched one from config. Add detectors.
                    Shared_mutex_file::Ptr arbiter = nullptr;
                    auto emplace_result = active_config.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(detected_port),
                            std::forward_as_tuple(detected_port, Port::SERIAL));
                    auto &current_port = emplace_result.first->second;
                    if (emplace_result.second && use_serial_arbiter) {
                        auto acquire_complete = Shared_mutex_file::Make_acquire_handler(
                                &Transport_detector::On_serial_acquired,
                                Shared_from_this(),
                                detected_port);
                        current_port.Create_arbiter(acquire_complete, worker);
                    }
                    for (auto &entry : configured_port.detectors) {
                        if (!Port_blacklisted(detected_port, entry.Get_handler())) {
                            // not in blacklist
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
        for (auto &entry : active_config) {
            entry.second.On_timer();
        }
    }

    return true;
}

Transport_detector::Port::Port(const std::string &name, Type ctype, Request_worker::Ptr w):
    name(name),
    current_detector(detectors.end()),
    re(name, platform_independent_filename_regex_matching_flag),
    worker(w),
    type(ctype)
{
}

Transport_detector::Port::Port(
    Socket_address::Ptr local_addr,
    Socket_address::Ptr peer_addr,
    Type type,
    Request_worker::Ptr w,
    int timeout):
    name(local_addr->Get_as_string()),
    local_addr(local_addr),
    peer_addr(peer_addr),
    current_detector(detectors.end()),
    re(name, platform_independent_filename_regex_matching_flag),
    worker(w),
    type(type),
    retry_timeout(std::chrono::seconds(timeout)),
    last_reopen(std::chrono::steady_clock::now() - retry_timeout)
{
}

Transport_detector::Port::~Port()
{
    socket_connecting_op.Abort();
    proxy_stream_reader_op.Abort();

    if (listener_stream && !listener_stream->Is_closed()) {
        listener_stream->Close();
    }
    listener_stream = nullptr;

    if (stream && !stream->Is_closed()) {
        stream->Close();
    }

    for (auto &it : sub_streams) {
        if (!it->Is_closed()) {
            it->Close();
        }
    }

    sub_streams.clear();

    stream = nullptr;
    arbiter = nullptr;
}

void
Transport_detector::Port::On_timer()
{
    if (stream && stream->Is_closed()) {
        // User has closed the stream.
        LOG("Port %s closed by user", stream->Get_name().c_str());
        stream = nullptr;
        if (arbiter) {
            arbiter->Release();
        }
        state = NONE;
    }

    // check for broken proxy connections.
    for (auto it = sub_streams.begin(); it != sub_streams.end();) {
        if ((*it)->Is_closed()) {
            it = sub_streams.erase(it);
        } else {
            it++;
        }
    }

    // Check if listener stream is still active
    if (type == TCP_IN || type == UDP_IN) {
        if (listener_stream && listener_stream->Is_closed()) {
            LOG("Reposting listener for stream %s", name.c_str());
            listener_stream = nullptr;
        }
    }

    if (state == NONE && std::chrono::steady_clock::now() - last_reopen > retry_timeout) {
        last_reopen = std::chrono::steady_clock::now();
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
                auto mode = Serial_processor_type::Stream::Mode().Baud(baud);

                stream = nullptr;
#               ifdef ANDROID
                Serial_processor_type::Get_instance()->Open(name, mode,
                    Make_callback([this](Io_stream::Ref s) { stream = s; },
                                  Io_stream::Ref()));
                if (!stream) {
                    VSM_EXCEPTION(Exception, "Serial stream opening failed");
                }
#               else
                stream = Serial_processor_type::Get_instance()->Open(name, mode);
#               endif
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
                    Connect_handler h = current_detector->Get_handler();
                    auto temp_handler = Make_callback(
                            [h](std::string name, int baud, Io_stream::Ref stream,
                                    Request::Ptr req)
                                    {
                        h.Set_args(name, baud, nullptr, stream);
                        h.Invoke();
                        req->Complete();
                                    },
                                    name,
                                    baud,
                                    stream,
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
Transport_detector::Port::Open_pipe()
{
    if (current_detector != detectors.end()) {
        try {
            stream = nullptr;
            stream = File_processor::Get_instance()->Open(name, "r+", false);
            LOG("Opened pipe %s", name.c_str());
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
                Connect_handler h = current_detector->Get_handler();
                auto temp_handler = Make_callback(
                    [h](std::string name, Io_stream::Ref stream,
                        Request::Ptr req)
                        {
                            h.Set_args(name, 0, nullptr, stream);
                            h.Invoke();
                            req->Complete();
                        },
                        name,
                        stream,
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
            LOG("Open failed for pipe %s", name.c_str());
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
        // no more detectors specified for this port, restart from the beginning.
        current_detector = detectors.begin();
        if (type != UDP_IN && type != TCP_IN) {
            // restart from beginning on next On_timer() call.
            state = NONE;
            return;
        }
        // In case of incoming connections there is no need to wait for On_timer.
    }

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
            worker);
        socket_connecting_op.Timeout(Transport_detector::TCP_CONNECT_TIMEOUT);
        break;
    case TCP_IN:
        if (listener_stream) {
            // Accept next connection.
            proxy_stream_reader_op = Socket_processor::Get_instance()->Accept(
                    listener_stream,
                Make_socket_accept_callback(
                    &Transport_detector::Port::Ip_connected,
                    this),
                worker);
        } else {
            socket_connecting_op.Abort();
            socket_connecting_op =
                Socket_processor::Get_instance()->Listen(
                local_addr,
                Make_socket_connect_callback(
                    &Transport_detector::Port::Listener_ready,
                    this),
                worker);
            socket_connecting_op.Timeout(Transport_detector::TCP_CONNECT_TIMEOUT);
        }
        break;
    case PROXY:
        socket_connecting_op.Abort();
        socket_connecting_op =
                Socket_processor::Get_instance()->Connect(
                peer_addr,
                Make_socket_connect_callback(
                    &Transport_detector::Port::Proxy_connected,
                    this),
                worker);
        socket_connecting_op.Timeout(Transport_detector::TCP_CONNECT_TIMEOUT);
        break;
    case UDP_IN:
        if (listener_stream) {
            // Accept next connection.
            proxy_stream_reader_op = Socket_processor::Get_instance()->Accept(
                    listener_stream,
                Make_socket_accept_callback(
                    &Transport_detector::Port::Ip_connected,
                    this),
                worker);
        } else {
            socket_connecting_op.Abort();
            socket_connecting_op =
                Socket_processor::Get_instance()->Bind_udp(
                local_addr,
                Make_socket_connect_callback(
                    &Transport_detector::Port::Listener_ready,
                    this),
                worker);
            socket_connecting_op.Timeout(Transport_detector::TCP_CONNECT_TIMEOUT);
        }
        break;
    case UDP_IN_ANY:
        socket_connecting_op.Abort();
        socket_connecting_op = Socket_processor::Get_instance()->Bind_udp(
            local_addr,
            Make_socket_listen_callback(
                &Transport_detector::Port::Ip_connected,
                this),
            worker);
        break;
    case UDP_OUT:
        socket_connecting_op.Abort();
        socket_connecting_op = Socket_processor::Get_instance()->Connect(
            peer_addr,
            Make_socket_connect_callback(
                &Transport_detector::Port::Ip_connected,
                this),
            worker,
            Io_stream::Type::UDP,
            local_addr);
        break;
    case CAN:
        socket_connecting_op.Abort();
        socket_connecting_op =
            Socket_processor::Get_instance()->Bind_can(
            name,
            std::vector<int>(),
            Make_socket_listen_callback(
                &Transport_detector::Port::Ip_connected,
                this),
            worker);
        break;
    case PIPE:
        Open_pipe();
        break;
    default:
        VSM_EXCEPTION(Internal_error_exception, "Reopen_and_call_next_handler: unsupported port type %d", type);
        state = NONE;
        break;
    }
}

void
Transport_detector::Port::On_proxy_data_received(
        ugcs::vsm::Io_buffer::Ptr buf,
        ugcs::vsm::Io_result result,
        Socket_stream::Ref idle_stream)
{
    if (result == Io_result::OK) {
        auto cmd = *(static_cast<const uint8_t*>(buf->Get_data()) + PROXY_SIGNATURE.size());
        if (memcmp(buf->Get_data(), PROXY_SIGNATURE.data(), PROXY_SIGNATURE.size())) {
            LOG("invalid data in proxy connection");
            state = NONE;
        } else {
            switch (cmd) {
            case PROXY_COMMAND_WAIT:
                proxy_stream_reader_op = stream->Read(
                    PROXY_RESPONSE_LEN,
                    PROXY_RESPONSE_LEN,
                    Make_read_callback(
                        &Transport_detector::Port::On_proxy_data_received,
                        this,
                        idle_stream),
                    worker);
                proxy_stream_reader_op.Timeout(std::chrono::seconds(PROXY_TIMEOUT));
                break;
            case PROXY_COMMAND_READY:
                LOG("proxy accepted connection");
                Ip_connected(idle_stream, Io_result::OK);
                break;
            case PROXY_COMMAND_NOTREADY:
                LOG("proxy denied connection");
                state = NONE;
                break;
            default:
                LOG("invalid proxy command");
                state = NONE;
                break;
            }
        }
    } else {
        LOG("proxy stream read failed");
        state = NONE;
    }
}

void
Transport_detector::Port::Proxy_connected(
        Socket_stream::Ref new_stream,
        Io_result result)
{
    if (result == Io_result::OK)
    {
        stream = new_stream;
        proxy_stream_reader_op = stream->Read(
            PROXY_RESPONSE_LEN,
            PROXY_RESPONSE_LEN,
            Make_read_callback(
                &Transport_detector::Port::On_proxy_data_received,
                this,
                new_stream),
            worker);
        proxy_stream_reader_op.Timeout(std::chrono::seconds(PROXY_TIMEOUT));
        auto id = Get_application_instance_id();
        auto hello_packet(PROXY_SIGNATURE);
        hello_packet.push_back(PROXY_COMMAND_HELLO);
        hello_packet.push_back((id >>  0) & 0xff);
        hello_packet.push_back((id >>  8) & 0xff);
        hello_packet.push_back((id >> 16) & 0xff);
        hello_packet.push_back((id >> 24) & 0xff);
        stream->Write(Io_buffer::Create(std::move(hello_packet)));
    } else {
        state = NONE;
    }
}

void
Transport_detector::Port::Listener_ready(
        Socket_stream::Ref new_stream,
        Io_result result)
{
    if (result == Io_result::OK)
    {
        listener_stream = new_stream;
        proxy_stream_reader_op = Socket_processor::Get_instance()->Accept(
            listener_stream,
            Make_socket_accept_callback(
                &Transport_detector::Port::Ip_connected,
                this),
            worker);
    } else {
        state = NONE;
    }
}

void
Transport_detector::Port::Ip_connected(
        Socket_stream::Ref new_stream,
        Io_result res)
{
    if (res == Io_result::OK)
    {
        if (type == TCP_IN) {
            sub_streams.push_back(new_stream);
        } else if (type == PROXY) {
            sub_streams.push_back(new_stream);
            stream = nullptr;
        } else if (type != UDP_IN) {
            state = CONNECTED;
            stream = new_stream;
        }
        Request::Ptr request = Request::Create();
        std::string address;
        if (new_stream->Get_type() == Io_stream::Type::TCP) {
            peer_addr = new_stream->Get_peer_address();
            address = new_stream->Get_peer_address()->Get_as_string();
        } else if (new_stream->Get_type() == Io_stream::Type::CAN) {
            address = name;
        } else {
            peer_addr = new_stream->Get_peer_address();
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
            new_stream->Close();
        }
        current_detector++;
        if (type == PROXY || type == TCP_IN || type == UDP_IN) {
            Reopen_and_call_next_handler();
        }
    } else {
        state = NONE;
    }
}

void
Transport_detector::Port::Protocol_not_detected(Io_stream::Ref str)
{
    if (type == PROXY || type == TCP_IN) {
        str->Close();
        for (auto it = sub_streams.begin(); it != sub_streams.end();) {
            if (*it == str) {
                it = sub_streams.erase(it);
            } else {
                it++;
            }
        }
    } else {
        if (stream == str) {
            Reopen_and_call_next_handler();
        }
    }
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
