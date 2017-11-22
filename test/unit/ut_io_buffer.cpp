// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Unit tests for Io_buffer class. */

#include <ugcs/vsm/io_buffer.h>

#include <UnitTest++.h>

#include <cstring>

using namespace ugcs::vsm;

TEST(basic_functionality)
{
    const char *test_str = "test";
    const size_t test_str_sz = strlen(test_str);

    /* Creation from string. */
    auto buf = Io_buffer::Create(test_str);
    CHECK_EQUAL(test_str_sz, buf->Get_length());
    CHECK_EQUAL(test_str, buf->Get_string());

    /* Creation from bytes array. */
    buf = Io_buffer::Create(test_str, test_str_sz);
    CHECK_EQUAL(test_str_sz, buf->Get_length());
    CHECK_EQUAL(test_str, buf->Get_string());

    /* Creation from bytes vector. */
    auto vec = std::make_shared<std::vector<uint8_t>>(test_str_sz);
    memcpy(&vec->front(), test_str, test_str_sz);
    buf = Io_buffer::Create(std::move(vec));
    CHECK_EQUAL(test_str_sz, buf->Get_length());
    CHECK_EQUAL(test_str, buf->Get_string());

    /* Creation from another buffer. */
    auto buf2 = Io_buffer::Create(*buf);
    CHECK_EQUAL(test_str_sz, buf2->Get_length());
    CHECK_EQUAL(test_str, buf2->Get_string());

    //XXX creation with offset/len

    //XXX slices

    //XXX concatenation

    //XXX empty buffers
}
