// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * socket_address.cpp
 *
 *  Created on: May 10, 2013
 *      Author: janis
 *
 *  Linux specific part of sockets implementation
 */

// This file should be built only on linux platforms
#include <ugcs/vsm/socket_address.h>

socklen_t
ugcs::vsm::Socket_address::Get_len()
{
        return sizeof(storage);
};
