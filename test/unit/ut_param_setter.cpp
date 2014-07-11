// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for parameters setters.
 */

#include <ugcs/vsm/param_setter.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

TEST(basic_functionality)
{
    int param1 = 0;
    const char *param2 = "";

    auto cb = Make_setter(param1, param2);
    cb->Get_arg<0>() = 10;
    cb->Get_arg<1>() = "test";
    cb();

    CHECK_EQUAL(10, param1);
    CHECK_EQUAL("test", param2);
}
