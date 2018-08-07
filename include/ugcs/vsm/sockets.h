// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * platform_sockets.h
 *
 * Defines platform specific parts of socket implementation.
 */

#ifndef _UGCS_VSM_SOCKETS_H_
#define _UGCS_VSM_SOCKETS_H_

#include <ugcs/vsm/platform_sockets.h>

namespace ugcs {
namespace vsm {
namespace sockets {

// These two calls are for WSAStartup and friends. Nop in unix world.
void
Init_sockets();
void
Done_sockets();

int
Create_socketpair(Socket_handle&, Socket_handle&);

int
Close_socket(Socket_handle);

bool
Is_last_operation_pending();

int
Make_nonblocking(Socket_handle);

int
Disable_sigpipe(Socket_handle);

int
Prepare_for_listen(Socket_handle, bool is_multicast);

} // namespace sockets
} // namespace vsm
} // namespace ugcs

#endif /* _UGCS_VSM_SOCKETS_H_ */
