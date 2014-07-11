// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * sockets.cpp
 *
 *  Created on: May 10, 2013
 *      Author: janis
 *
 *  Linux specific part of sockets implementation
 */

// This file should be built only on windows platforms
#if _WIN32

#include <ugcs/vsm/sockets.h>
#include <ugcs/vsm/exception.h>

void
ugcs::vsm::sockets::Init_sockets()
{
    static WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
        VSM_EXCEPTION(ugcs::vsm::Internal_error_exception, "WSAStartup failed: error %d", WSAGetLastError());
}

void
ugcs::vsm::sockets::Done_sockets()
{
    if (WSACleanup() != 0)
        VSM_EXCEPTION(ugcs::vsm::Internal_error_exception, "WSACleanup failed: error %d", WSAGetLastError());
}

int
ugcs::vsm::sockets::Create_socketpair(Socket_handle& sock1, Socket_handle& sock2)
{
    sock1 = sock2 = INVALID_SOCKET;
    union {
       struct sockaddr_in inaddr;
       struct sockaddr addr;
    } a;
    SOCKET listener;
    int e;
    socklen_t addrlen = sizeof(a.inaddr);

    do {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
            break;

        memset(&a, 0, sizeof(a));
        a.inaddr.sin_family = AF_INET;
        a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.inaddr.sin_port = 0;  // Let the system to pick random port.

        if (bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        memset(&a, 0, sizeof(a));
        if (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
            break;
        a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.inaddr.sin_family = AF_INET;

        if (listen(listener, 1) == SOCKET_ERROR)
            break;

        sock1 = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (sock1 == INVALID_SOCKET)
            break;
        if (connect(sock1, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        sock2 = accept(listener, NULL, NULL);
        if (sock2 == INVALID_SOCKET)
            break;

        closesocket(listener);
        return 0;

    } while (0);

    e = WSAGetLastError();
    closesocket(listener);
    closesocket(sock1);
    closesocket(sock2);
    WSASetLastError(e);
    return SOCKET_ERROR;
}

int
ugcs::vsm::sockets::Close_socket(Socket_handle s)
{
    return closesocket(s);
}

bool
ugcs::vsm::sockets::Is_last_operation_pending()
{
    int e = WSAGetLastError();
    return (e == WSAEWOULDBLOCK);
}

int
ugcs::vsm::sockets::Make_nonblocking(Socket_handle handle)
{
    unsigned long arg = 1;
    return ioctlsocket(handle, FIONBIO, &arg);
}

int
ugcs::vsm::sockets::Prepare_for_listen(Socket_handle)
{
    // do not try to set SO_REUSEADDR on windows.
    return 0;
}

int
ugcs::vsm::sockets::Disable_sigpipe(Socket_handle)
{
    return 0;
}

#endif // _WIN32
