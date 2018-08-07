// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * socket_address.cpp
 *
 *  Created on: Sep 9, 2013
 *      Author: janis
 */

#include <ugcs/vsm/socket_address.h>
#include <cstring>

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

size_t
std::hash<Socket_address::Ptr>::operator() (const Socket_address::Ptr& a) const
{
    size_t ret;
    auto sa = a->Get_sockaddr_ref();
    if (sa->sa_family == AF_INET) {
        auto si = reinterpret_cast<sockaddr_in*>(sa);
        ret = si->sin_port;
        ret <<= 16;
        ret += si->sin_addr.s_addr;
    } else if (sa->sa_family == AF_INET6) {
        auto si6 = reinterpret_cast<sockaddr_in6*>(sa);
        ret = si6->sin6_port;
        ret <<= 8;
        ret += si6->sin6_addr.s6_addr[15];
        ret <<= 8;
        ret += si6->sin6_addr.s6_addr[14];
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Only IPv4 and IPv6 supported.");
    }
    return ret;
}

bool
std::equal_to<Socket_address::Ptr>::operator()(const Socket_address::Ptr& lhs, const Socket_address::Ptr& rhs) const
{
    auto l = lhs->Get_sockaddr_ref();
    auto r = rhs->Get_sockaddr_ref();
    if (l->sa_family != r->sa_family) {
        return false;
    }
    if (l->sa_family == AF_INET) {
        auto li = reinterpret_cast<sockaddr_in*>(l);
        auto ri = reinterpret_cast<sockaddr_in*>(r);
        return (li->sin_addr.s_addr == ri->sin_addr.s_addr && li->sin_port == ri->sin_port);
    } else if (l->sa_family == AF_INET6) {
        auto li = reinterpret_cast<sockaddr_in6*>(l);
        auto ri = reinterpret_cast<sockaddr_in6*>(r);
        if (li->sin6_port == ri->sin6_port) {
            return memcmp(li->sin6_addr.s6_addr, ri->sin6_addr.s6_addr, 16) == 0;
        }
    } else {
        VSM_EXCEPTION(Invalid_param_exception, "Only IPv4 and IPv6 supported.");
    }
    return false;
}

Socket_address::~Socket_address()
{
}

void
Socket_address::Set(addrinfo* source)
{
    std::memcpy(&storage, source->ai_addr, sizeof(sockaddr));
    if (!is_resolved) {
        name = std::string(inet_ntoa(As_sockaddr_in().sin_addr));
        service = std::to_string(ntohs(As_sockaddr_in().sin_port));
    }
    is_resolved = true;
}

void
Socket_address::Set(const sockaddr_storage& data)
{
    storage = data;
    if (!is_resolved) {
        name = std::string(inet_ntoa(As_sockaddr_in().sin_addr));
        service = std::to_string(ntohs(As_sockaddr_in().sin_port));
    }
    is_resolved = true;
}

void
Socket_address::Set(const sockaddr_in& source)
{
    std::memcpy(&storage, &source, sizeof(sockaddr_in));
    if (!is_resolved) {
        name = std::string(inet_ntoa(As_sockaddr_in().sin_addr));
        service = std::to_string(ntohs(As_sockaddr_in().sin_port));
    }
    is_resolved = true;
}

void
Socket_address::Set(const std::string& address, const std::string& port)
{
    name = address;
    service = port;
    addrinfo* result;
    addrinfo hint = {AI_NUMERICHOST, AF_INET, 0, 0, 0, nullptr, nullptr, nullptr};
    if (getaddrinfo(address.c_str(), port.c_str(), &hint, &result) == 0) {
        is_resolved = true;
        Set(result);
        freeaddrinfo(result);
    } else {
        is_resolved = false;
    }
}

void
Socket_address::Set_service(const std::string& port)
{
    service = port;
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

sockaddr_in6&
Socket_address::As_sockaddr_in6()
{
    return reinterpret_cast<sockaddr_in6&>(storage);
}

sockaddr_in
Socket_address::Get_as_sockaddr_in()
{
    return *reinterpret_cast<sockaddr_in*>(&storage);
}

std::string
Socket_address::Get_address_as_string()
{
    if (is_resolved)
    {
        auto addr = inet_ntoa(As_sockaddr_in().sin_addr);
        if (addr) {
            return std::string(addr);
        }
    }
    return std::string();
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

bool
Socket_address::Is_multicast_address()
{
    if (!is_resolved) {
        return false;
    }
    if ((As_sockaddr_in().sin_addr.s_addr & 0xF0) != 0xE0) {
        return false;
    }

    if (As_sockaddr_in().sin_addr.s_addr == 0x000000E0) {
        return false;
    }

    return true;
}

bool
Socket_address::Is_loopback_address()
{
    if (!is_resolved) {
        return false;
    }
    if ((As_sockaddr_in().sin_addr.s_addr & 0x7F) == 0x7F) {
        return true;
    }
    return false;
}
