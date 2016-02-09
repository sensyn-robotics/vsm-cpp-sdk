// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
/*
 * service_discovery_processor.cpp
 *
 *  Created on: May 5, 2015
 *      Author: j
 */

#include <ugcs/vsm/service_discovery_processor.h>
#include <ugcs/vsm/param_setter.h>
#include <ugcs/vsm/http_parser.h>
#include <sstream>

namespace ugcs {
namespace vsm {

Singleton<Service_discovery_processor> Service_discovery_processor::singleton;
std::string Service_discovery_processor::DEFAULT_DISCOVERY_ADDRESS("239.198.46.46");
std::string Service_discovery_processor::DEFAULT_DISCOVERY_PORT("1991");
std::string Service_discovery_processor::LOCAL_ADDRESS_IDENTIFIER("{local_address}");
std::string Service_discovery_processor::SEARCH_METHOD_STRING("M-SEARCH");
std::string Service_discovery_processor::NOTIFY_METHOD_STRING("NOTIFY");

std::string Service_discovery_processor::LOOPBACK_IDENTIFIER("lb");
std::string Service_discovery_processor::MC_IDENTIFIER("mc");
std::string Service_discovery_processor::LOOPBACK_BROADCAST_ADDRESS("127.255.255.255");

Service_discovery_processor::Service_discovery_processor(Socket_address::Ptr muticast_adress):
        Request_processor("Service discovery processor"),
        multicast_adress(muticast_adress)
{
    loopback_broadcast_adress =
        Socket_address::Create(
                LOOPBACK_BROADCAST_ADDRESS,
                muticast_adress->Get_service_as_c_str());
    // Get the unique instance identifier.
    my_instance_identifier = std::to_string(Get_application_instance_id());
}

Service_discovery_processor::~Service_discovery_processor()
{
}

void
Service_discovery_processor::On_enable()
{
    Request_processor::On_enable();

    worker = Request_worker::Create(
        "Service discovery worker",
        std::initializer_list<Request_container::Ptr>{Shared_from_this()});
    worker->Enable();

    my_timer = Timer_processor::Get_instance()->Create_timer(
            std::chrono::seconds(5),
            Make_callback(&Service_discovery_processor::On_timer, Shared_from_this()),
            worker);
}

void
Service_discovery_processor::On_disable()
{
    my_timer->Cancel();
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_disable_impl,
            Shared_from_this(),
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
    request->Wait_done();
    Set_disabled();
    worker->Disable();
    worker = nullptr;
}

void
Service_discovery_processor::Schedule_read(std::string stream_id)
{
    My_socket* my_port = nullptr;
    if (stream_id == MC_IDENTIFIER) {
        my_port = &receiver;
    } else if (stream_id == LOOPBACK_IDENTIFIER) {
        my_port = &sender_loopback;
    } else {
        auto sender = sender_sockets.find(stream_id);
        if (sender != sender_sockets.end()) {
            my_port = &sender->second;
        }
    }

    if (my_port && worker->Is_enabled()) {
    my_port->first = my_port->second->Read_from(
            1000,
            Make_callback(
                &Service_discovery_processor::On_read, Shared_from_this(),
                Io_buffer::Ptr(),
                Io_result(),
                Socket_address::Ptr(),
                stream_id),
            worker);
    }
}

void
Service_discovery_processor::On_read(
        Io_buffer::Ptr buffer,
        Io_result result,
        Socket_address::Ptr addr,
        std::string stream_id)
{
    if (result != Io_result::OK) {
        return;
    }

    Http_parser parser;
    std::istringstream buf(buffer->Get_string());
    parser.Parse(buf);

//    LOG("read(%s)=%s", stream_id.c_str(), buffer->Get_string().c_str());

    if (parser.Get_method() == SEARCH_METHOD_STRING) {
        auto type = parser.Get_header_value("st");
        if (!type.empty()) {
            for (auto &service : my_services) {
                if (std::get<0>(service) == type) {
                    // Use receiver socket to send the response.
                    if (Has_location_string(std::get<2>(service))) {
                        if (addr->Is_loopback_address()) {
                            Send_response(
                                addr,
                                std::get<0>(service),
                                std::get<1>(service),
                                Build_location_string(
                                    std::get<2>(service),
                                    sender_loopback.second->Get_local_address()));
                        } else {
                            // If we need to replace {local_address} then send
                            // response for each possible location.
                            // This is because there is no easy way (let alone
                            // platform independent way) to get incoming interface.
                            for (auto& iface : sender_sockets) {
                                if (iface.second.second) {
                                    Send_response(
                                        addr,
                                        std::get<0>(service),
                                        std::get<1>(service),
                                        Build_location_string(
                                            std::get<2>(service),
                                            iface.second.second->Get_local_address()));
                                }
                            }
                        }
                    } else {
                        Send_response(
                                addr,
                                std::get<0>(service),
                                std::get<1>(service),
                                std::get<2>(service));
                    }
                }
            }
        }
    }

    if (parser.Get_method() == NOTIFY_METHOD_STRING || parser.Get_method().empty()) {
        std::string service_type;
        bool active = false, not_active = false;
        if (parser.Get_method().empty()) {
            // It's a M-SEARCH response.
            service_type = parser.Get_header_value("ST");
            active = true;
        } else {
            service_type = parser.Get_header_value("NT");
            // See if service is becoming alive or going offline.
            active = parser.Get_header_value("NTS").find("ssdp:alive") != std::string::npos;
            not_active = parser.Get_header_value("NTS").find("ssdp:byebye") != std::string::npos;
        }
        // Find the advertised service type in our list.
        auto service = subscribed_services.find(service_type);
        if (service != subscribed_services.end() && (active || not_active)) {
            // call registered callback on given type.
            auto ctx = service->second.second;
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
                Request::Ptr request = Request::Create();
                auto temp_handler = Make_callback(
                        []( std::string type,
                            std::string name,
                            std::string location,
                            std::string id,
                            bool active,
                            Detection_handler h,
                            Request::Ptr request)
                                {
                                    h.Set_args(type, name, location, id, active);
                                    h.Invoke();
                                    request->Complete();
                                },
                                service_type,
                                parser.Get_header_value("usn"),
                                parser.Get_header_value("location"),
                                parser.Get_header_value("ID"),
                                active,
                                service->second.first,  // handler
                                request);
                request->Set_processing_handler(temp_handler);
                ctx->Submit_request_locked(request, std::move(ctx_lock));
            }
        }
    }
    Schedule_read(stream_id);
}

void
Service_discovery_processor::On_sender_bound(Socket_processor::Stream::Ref stream, Io_result result)
{
    auto sender = sender_sockets.find(stream->Get_local_address()->Get_address_as_string());
    if (sender != sender_sockets.end()) {
        if (result == Io_result::OK) {
            sender->second.second = stream;
            for (auto &service : my_services) {
                Send_notify(stream, multicast_adress, std::get<0>(service), std::get<1>(service), std::get<2>(service), true);
            }
            for (auto &service : subscribed_services) {
                Send_msearch(stream, multicast_adress, service.first);
            }
            Schedule_read(stream->Get_local_address()->Get_address_as_string());
        } else {
            LOG_ERR("Failed to bind local listener port %s", multicast_adress->Get_service_as_c_str());
            sender_sockets.erase(sender);
        }
    }
}

bool
Service_discovery_processor::On_timer()
{
    if (my_services.size() + subscribed_services.size() == 0) {
        // No services. nothing to do here.
        return true;
    }
    auto locals = Socket_processor::Enumerate_local_interfaces();

    // Remove outgoing streams of removed interfaces.
    bool found = false;
    for (auto sender = sender_sockets.begin(); sender != sender_sockets.end();) {
        for (auto& iface : locals) {
            if (iface.is_multicast && !iface.is_loopback) {
                for (auto& local : iface.adresses) {
                    if ((*sender).first == local->Get_address_as_string()) {
                        found = true;
                    }
                }
            }
        }
        if (found) {
            sender++;
        } else {
            LOG("Lost local address %s", (*sender).first.c_str());
            if ((*sender).second.second) {
                (*sender).second.second->Close();
            }
            sender = sender_sockets.erase(sender);
        }
    }

    // Add newly discovered interfaces to our multicast streams.
    for (auto& iface : locals) {
        if (iface.is_multicast && !iface.is_loopback)
        {
            found = false;
            for (auto& local : iface.adresses) {
                if (sender_sockets.find(local->Get_address_as_string()) != sender_sockets.end()) {
                    found = true;
                    break;
                }
            }
            if (!found && !iface.adresses.empty()) {
                auto addr = iface.adresses.front();
                // Create listener on first IP on newly discovered interface.
                // Must bind on arbitrary port to avoid conflict with other discoverer services on this host.
                // Unicast discovery responses will come back to this socket.
                addr->Set_service("0");
                LOG("Discovered new local address %s", addr->Get_address_as_string().c_str());
                sender_sockets.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(addr->Get_address_as_string()),
                        std::forward_as_tuple(Operation_waiter(), nullptr));
                Socket_processor::Get_instance()->Bind_udp(
                        addr,
                        Make_socket_listen_callback(&Service_discovery_processor::On_sender_bound, Shared_from_this()),
                        worker);
            }
        }
    }
    return true;
}

bool
Service_discovery_processor::Has_location_string(const std::string& loc)
{
    auto pos = loc.find(LOCAL_ADDRESS_IDENTIFIER);
    return (pos != std::string::npos);
}

std::string
Service_discovery_processor::Build_location_string(const std::string& loc, Socket_address::Ptr local_addr)
{
    auto pos = loc.find(LOCAL_ADDRESS_IDENTIFIER);
    if (pos == std::string::npos) {
        return loc;
    } else {
        std::string ret(loc);
        ret.replace(pos, LOCAL_ADDRESS_IDENTIFIER.length(), local_addr->Get_address_as_string());
        return ret;
    }
}

void
Service_discovery_processor::Advertise_service(
        const std::string& type,
        const std::string& name,
        const std::string& location)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_advertise,
            Shared_from_this(),
            type,
            name,
            location,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

void
Service_discovery_processor::Unadvertise_service(
        const std::string& type,
        const std::string& name,
        const std::string& location)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_unadvertise,
            Shared_from_this(),
            type,
            name,
            location,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

auto Dump_error =
    [](Io_result result, std::string data)
    {
        if (result != Io_result::OK) {
            LOG_ERR("IO_error %s", data.c_str());
        }
    };

void
Service_discovery_processor::Send_response(
        Socket_address::Ptr addr,
        const std::string& type,
        const std::string& name,
        const std::string& location)
{
    auto response = std::string("HTTP/1.1 200 OK\r\nST:");
    response += type;
    response += "\r\nUSN:";
    response += name;
    response += "\r\nLocation:";
    response += location;
    response += "\r\nID:";
    response += my_instance_identifier;
    response += "\r\n\r\n";
    receiver.second->Write_to(
            Io_buffer::Create(response),
            addr,
            Make_write_callback(Dump_error, "Send_response"));
}

void
Service_discovery_processor::Send_msearch(
        Socket_processor::Stream::Ref stream,
        Socket_address::Ptr dest_addr,
        const std::string& type)
{
    if (stream) {
        auto notify = SEARCH_METHOD_STRING;
        notify += " * HTTP/1.1\r\nHOST:";
        notify += dest_addr->Get_as_string();
        notify += "\r\nMAN: \"ssdp:discover\"\r\nMX: 3\nST:";
        notify += type;
        notify += "\r\n\r\n";
        // Send search. Do not care about the Write result.
        stream->Write_to(Io_buffer::Create(notify),
                dest_addr,
                Make_write_callback(Dump_error, dest_addr->Get_as_string()));
    }
}

void
Service_discovery_processor::Subscribe_for_service(
        const std::string &type,
        Detection_handler handler,
        Request_processor::Ptr context)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_subscribe_for_service,
            Shared_from_this(),
            type,
            handler,
            context,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

void
Service_discovery_processor::Search_for_service(
        const std::string &type)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_search_for_service,
            Shared_from_this(),
            type,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

void
Service_discovery_processor::Unsubscribe_from_service(
        const std::string &type)
{
    Request::Ptr request = Request::Create();
    auto proc_handler = Make_callback(
            &Service_discovery_processor::On_unsubscribe_from_service,
            Shared_from_this(),
            type,
            request);
    request->Set_processing_handler(proc_handler);
    Submit_request(request);
}

bool
Service_discovery_processor::Activate()
{
    if (my_services.size() + subscribed_services.size() == 1) {
        // Bind multicast listener on all interfaces.
        auto multicast_listener_address = Socket_address::Create("0.0.0.0", multicast_adress->Get_service_as_c_str());
        auto loopback_sender_address = Socket_address::Create("127.0.0.1", "0");

        // Bind synchronously.
        Socket_processor::Get_instance()->Bind_udp(
            multicast_listener_address,
            Make_socket_listen_callback([&](Socket_processor::Stream::Ref s, Io_result result)
                {
                    if (result == Io_result::OK) {
                        receiver.second = s;
                        s->Add_multicast_group(multicast_listener_address, multicast_adress);
                        Schedule_read(MC_IDENTIFIER);
                    } else {
                        LOG_ERR("Failed to bind multicast listener on port %s", multicast_adress->Get_service_as_c_str());
                    }
                }),
            Request_temp_completion_context::Create(),
            true);

        // Assume loopback is always present (and always be).
        // I.e. do not support dynamic addtion/removal of loopback interface
        // as we do for other interfaces.
        // Bind loopback sender to loopback ip and to arbitrary port.
        // This assumes that your loopback ip is 127.0.0.1.
        Socket_processor::Get_instance()->Bind_udp(
            loopback_sender_address,
            Make_socket_listen_callback([&](Socket_processor::Stream::Ref s, Io_result result)
                {
                    if (result == Io_result::OK) {
                        sender_loopback.second = s;
                        sender_loopback.second->Enable_broadcast(true);
                        Schedule_read(LOOPBACK_IDENTIFIER);
                    } else {
                        LOG_ERR("Failed to bind loopback sender");
                    }
                }),
            Request_temp_completion_context::Create(),
            true);

        On_timer();
        return false;
    } else {
        return true;
    }
}

void
Service_discovery_processor::Deactivate_if_no_services()
{
    if (my_services.size() + subscribed_services.size()) {
        // Some services still present.
        return;
    }
    // All services gone. Remove the sockets.
    sender_loopback.first.Abort();
    if (sender_loopback.second) {
        sender_loopback.second->Close();
    }
    receiver.first.Abort();
    if (receiver.second) {
        receiver.second->Close();
    }
    for (auto &stream : sender_sockets) {
        stream.second.first.Abort();
        if (stream.second.second) {
            stream.second.second->Close();
        }
    }
    sender_sockets.clear();
}

void
Service_discovery_processor::On_advertise(
        const std::string type,
        const std::string name,
        const std::string location,
        Request::Ptr request)
{
    request->Complete();
    my_services.emplace(std::make_tuple(type, name, location));

    if (Activate()) {
        for (auto& stream : sender_sockets) {
            Send_notify(stream.second.second, multicast_adress, type, name, location, true);
        }
    }
    Send_notify(sender_loopback.second, loopback_broadcast_adress, type, name, location, true);
}

void
Service_discovery_processor::Send_notify(
        Socket_processor::Stream::Ref stream,
        Socket_address::Ptr dest_addr,
        const std::string& type,
        const std::string& name,
        const std::string& location,
        bool alive)
{
    if (stream) {
        auto notify = NOTIFY_METHOD_STRING;
        notify += " * HTTP/1.1\r\nHOST:";
        notify += dest_addr->Get_as_string();
        if (alive) {
            notify += "\r\nNTS:ssdp:alive\r\nNT:";
        } else {
            notify += "\r\nNTS:ssdp:byebye\r\nNT:";
        }
        notify += type;
        notify += "\r\nUSN:";
        notify += name;
        notify += "\r\nID:";
        notify += my_instance_identifier;
        notify += "\r\nLocation:";
        notify += Build_location_string(location, stream->Get_local_address());
        notify += "\r\n\r\n";
        // Send notification. Do not care about Write result.
        stream->Write_to(Io_buffer::Create(notify),
                dest_addr,
                Make_write_callback(Dump_error, "Send_notify"));
    }
}

void
Service_discovery_processor::On_unadvertise(
        const std::string type,
        const std::string name,
        const std::string location,
        Request::Ptr request)
{

    auto result = my_services.erase(std::make_tuple(type, name, location));
    request->Complete();

    if (result) {
        for (auto& stream : sender_sockets) {
            Send_notify(stream.second.second, multicast_adress, type, name, location, false);
        }
        Send_notify(sender_loopback.second, loopback_broadcast_adress, type, name, location, false);
        Deactivate_if_no_services();
    }
}

void
Service_discovery_processor::On_subscribe_for_service(
        const std::string type,
        Detection_handler handler,
        Request_processor::Ptr context,
        Request::Ptr request)
{
    subscribed_services[type] = std::make_pair(handler, context);
    request->Complete();
    if (Activate()) {
        for (auto& stream : sender_sockets) {
            Send_msearch(stream.second.second, multicast_adress, type);
        }
    }
    Send_msearch(sender_loopback.second, loopback_broadcast_adress, type);
}

void
Service_discovery_processor::On_unsubscribe_from_service(
        const std::string type,
        Request::Ptr request)
{
    auto result = subscribed_services.erase(type);
    request->Complete();
    if (result) {
        Deactivate_if_no_services();
    }
}

void
Service_discovery_processor::On_search_for_service(
        const std::string type,
        Request::Ptr request)
{
    if (request) {
        request->Complete();
    }
    if (subscribed_services.size()) {
        Send_msearch(sender_loopback.second, loopback_broadcast_adress, type);
        for (auto& stream : sender_sockets) {
            Send_msearch(stream.second.second, multicast_adress, type);
        }
    }
}

void
Service_discovery_processor::On_disable_impl(Request::Ptr request)
{
    my_services.clear();
    subscribed_services.clear();
    Deactivate_if_no_services();
    request->Complete();
}

} /* namespace vsm */
} /* namespace ugcs */
