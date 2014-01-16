// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file socket_address.h
 *
 * Socket address representation.
 */

#ifndef SOCKET_ADDRESS_H_
#define SOCKET_ADDRESS_H_

#include <string>
#include <vsm/utils.h>
#include <vsm/reference_guard.h>
#include <vsm/platform_sockets.h>

namespace vsm
{

/** Wrapper class for sockaddr_in structure
 * Socket_address is based on struct sockaddr and friends,
 * so it can be used directly in socket calls.
 */
class Socket_address: public std::enable_shared_from_this<Socket_address>
{
    DEFINE_COMMON_CLASS(Socket_address, Socket_address)
public:
    Socket_address();
    Socket_address(const Socket_address::Ptr);
    Socket_address(const sockaddr_storage&);
    Socket_address(const sockaddr_in&);
    Socket_address(const std::string&, const std::string&);

    virtual ~Socket_address();

    /** Set the generic address
     */
    void
    Set(const sockaddr_storage&);

    /** For use with getaddrinfo()  */
    void
    Set(addrinfo*);

    void
    Set(const sockaddr_in&);

    void
    Set(const std::string&, const std::string&);

    sockaddr_storage
    Get_as_sockaddr_storage();

    sockaddr_in
    Get_as_sockaddr_in();

    /** Get string representation as "xxx.xxx.xxx.xxx:portnumber". */
    std::string
    Get_as_string();

    /** Get address for use in socket calls, eg getaddrinfo(). */
    const char *
    Get_name_as_c_str();

    /** Get service for use in socket calls, eg getaddrinfo(). */
    const char *
    Get_service_as_c_str();

    /** TRUE when address has been set by one of Set_* functions. */
    bool
    Is_valid() {return is_resolved;};

    /** Use this function to pass this structure to recvfrom and similar
     * Which require sockaddr*
     */
    sockaddr*
    Get_sockaddr_ref();

    /** Use this function to pass this structure to recvfrom and similar
     * Which require legth of address structure.
     */
    socklen_t
    Get_len(){return sizeof(storage);};

private:
    sockaddr_in&
    As_sockaddr_in();

    sockaddr_storage storage;

    std::string name;
    std::string service;

    bool is_resolved;
};

} /* namespace vsm */

#endif /* SOCKET_ADDRESS_H_ */
