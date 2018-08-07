// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * serial_processor.cpp
 *
 *  Created on: Jul 17, 2013
 *      Author: Janis
 */

#include <iostream>

#include <ugcs/vsm/vsm.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

TEST(socket_processor_listen_accept)
{
    std::cout << "Enumerating serial ports:" << std::endl;
    auto v = Serial_processor::Enumerate_port_names();
    for (auto i : v)
    {
        std::cout << i << std::endl;
    }
}
