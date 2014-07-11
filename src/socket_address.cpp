// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * socket_address.cpp
 *
 *  Created on: Sep 9, 2013
 *      Author: janis
 */

#include <cstring>
#include <ugcs/vsm/socket_address.h>

using namespace ugcs::vsm;

Socket_address::Socket_address():name(), service(), is_resolved(false)
{
    std::memset(&storage, 0, sizeof(storage));
}

Socket_address::Socket_address(const Socket_address::Ptr data)
{
    if (data) {
        storage = data->storage;
        is_resolved = data->is_resolved;
        name = data->name;
        service = data->service;
    } else {
        Socket_address();
    }
}

Socket_address::Socket_address(const sockaddr_storage& data)
{
    Set(data);
}

Socket_address::Socket_address(const sockaddr_in& source)
{
    Set(source);
}

Socket_address::Socket_address(const std::string& address, const std::string& port)
{
    Set(address, port);
}

Socket_address::~Socket_address()
{
}

void
Socket_address::Set(addrinfo* source)
{
    std::memcpy(&storage, source->ai_addr, sizeof(sockaddr));
    is_resolved = true;
}

void
Socket_address::Set(const sockaddr_storage& data)
{
    storage = data;
    name = std::string();
    service = std::string();
    is_resolved = true;
}

void
Socket_address::Set(const sockaddr_in& source)
{
    std::memcpy(&storage, &source, sizeof(sockaddr_in));
    is_resolved = true;
    name = std::string();
    service = std::string();
}

void
Socket_address::Set(const std::string& address, const std::string& port)
{
    name = address;
    service = port;
    addrinfo* result;
    addrinfo hint = {AI_NUMERICHOST, AF_INET, 0, 0, 0, nullptr, nullptr, nullptr};
    if (getaddrinfo(address.c_str(), port.c_str(), &hint, &result) == 0) {
        Set(result);
        is_resolved = true;
        freeaddrinfo(result);
    } else {
        is_resolved = false;
    }
}

sockaddr_storage
Socket_address::Get_as_sockaddr_storage()
{
    return storage;
}

sockaddr_in&
Socket_address::As_sockaddr_in()
{
    return reinterpret_cast<sockaddr_in&>(storage);
}

sockaddr_in
Socket_address::Get_as_sockaddr_in()
{
    return *reinterpret_cast<sockaddr_in*>(&storage);
}

std::string
Socket_address::Get_as_string()
{
    if (is_resolved)
    {
        if (name.length() == 0) {
            auto addr = inet_ntoa(As_sockaddr_in().sin_addr);
            if (addr) {
                name = std::string(addr);
            }
        }
        if (service.length() == 0) {
            service = std::to_string(ntohs(As_sockaddr_in().sin_port));
        }
    }
    return name + ":" + service;
}

sockaddr*
Socket_address::Get_sockaddr_ref()
{
    return reinterpret_cast<sockaddr*>(&storage);
}

void
Socket_address::Set_resolved(bool r)
{
    is_resolved = r;
}

const char *
Socket_address::Get_name_as_c_str()
{
    return name.c_str();
}

/** Get service for use in socket calls, eg getaddrinfo(). */
const char *
Socket_address::Get_service_as_c_str()
{
    return service.c_str();
}
