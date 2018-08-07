// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Reference_guard class tests.
 */

#include <ugcs/vsm/reference_guard.h>

#include <memory>

#include <UnitTest++.h>

using namespace ugcs::vsm;

class A {
public:
    int ref_count = 0;

    void
    Add_ref()
    {
        ref_count++;
    }

    void
    Release_ref()
    {
        CHECK(ref_count > 0);
        ref_count--;
    }
};

class B: public A {
public:

};

TEST(basic_functionality)
{
    auto a = std::make_shared<A>();
    auto b = std::make_shared<B>();

    {
        Reference_guard<decltype(a)> ref_a(a);
        CHECK_EQUAL(1, a->ref_count);
    }
    CHECK_EQUAL(0, a->ref_count);

    {
        Reference_guard<decltype(a)> ref_a_b(b);
        CHECK_EQUAL(1, b->ref_count);
    }
    CHECK_EQUAL(0, b->ref_count);

    {
        Reference_guard<decltype(b)> ref_b(b);
        CHECK_EQUAL(1, b->ref_count);

        Reference_guard<decltype(a)> ref_a_b(ref_b);
        CHECK_EQUAL(2, b->ref_count);

        Reference_guard<decltype(b)> ref_b_2(ref_b);
        CHECK_EQUAL(3, b->ref_count);

        Reference_guard<decltype(b)> ref_b_3(std::move(ref_b));
        CHECK_EQUAL(3, b->ref_count);
        CHECK(!ref_b);

        ref_b_3 = nullptr;
        CHECK_EQUAL(2, b->ref_count);
        CHECK(!ref_b_3);

        ref_a_b = std::move(ref_b_2);
        CHECK_EQUAL(1, b->ref_count);
        CHECK(!ref_b_2);
    }
    CHECK_EQUAL(0, b->ref_count);
}
