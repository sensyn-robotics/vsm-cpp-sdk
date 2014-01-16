// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * serial_processor.cpp
 *
 *  Created on: Jul 17, 2013
 *      Author: Janis
 */


#include <iostream>

#include <vsm/vsm.h>
#include <vsm/callback.h>
#include <vsm/debug.h>
#include <vsm/param_setter.h>

#include <UnitTest++.h>

using namespace vsm;

class Test_case_wrapper
{
public:
    Test_case_wrapper() {
        Initialize("vsm.conf", std::ios_base::in);
    }

    ~Test_case_wrapper() {
        Terminate();
    }
};

TEST_FIXTURE(Test_case_wrapper, socket_processor_listen_accept)
{
    auto sp = Serial_processor::Get_instance();
    auto worker = Request_worker::Create("UT serial prcessor worker");
    CHECK(sp->Is_enabled());
    worker->Enable();

    std::cout << "Enumerating serial ports:" << std::endl;
    auto v = sp->Enumerate_port_names();
    for (auto i : v)
    {
        std::cout << i << std::endl;
    }

    worker->Disable();
}
