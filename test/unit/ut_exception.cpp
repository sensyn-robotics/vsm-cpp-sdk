// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Test for VSM exceptions.
 */

#include <ugcs/vsm/exception.h>
#include <ugcs/vsm/debug.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

TEST(basic_exception)
{
    bool cought = false;
    try {
        VSM_EXCEPTION(Exception, "Test exception %d", 237);
    } catch (const ugcs::vsm::Exception &e) {
        std::string s = e.what();
        CHECK(s.find("Test exception 237") != std::string::npos);
        cought = true;
    }
    CHECK(cought);
}

TEST(custom_exception)
{
    class A {
    public:
        VSM_DEFINE_EXCEPTION(My_exception);
    };
    class B {
    public:
        VSM_DEFINE_EXCEPTION(My_exception);
    };

    bool cought = false;
    try {
        VSM_EXCEPTION(A::My_exception, "Test exception %d", 237);
    } catch (const A::My_exception &e) {
        std::string s = e.what();
        CHECK(s.find("Test exception 237") != std::string::npos);
        cought = true;
    } catch (const B::My_exception &e) {
        CHECK(false);
    }
    CHECK(cought);
}

TEST(param_exception)
{
    VSM_DEFINE_EXCEPTION(My_exception, const char *);

    bool cought = false;
    try {
        VSM_PARAM_EXCEPTION(My_exception, "param", "Test exception %d", 237);
    } catch (const My_exception &e) {
        std::string s = e.what();
        CHECK(s.find("Test exception 237") != std::string::npos);
        CHECK_EQUAL("param", e.param);
        cought = true;
    }
    CHECK(cought);
}

TEST(derived_exception)
{
    VSM_DEFINE_EXCEPTION(My_exception, const char *);
    VSM_DEFINE_DERIVED_EXCEPTION(My_exception, My_derived_exception_A);
    VSM_DEFINE_DERIVED_EXCEPTION(My_exception, My_derived_exception_B);

    bool cought = false;
    try {
        VSM_PARAM_EXCEPTION(My_derived_exception_A, "param", "Test exception %d", 237);
    } catch (const My_derived_exception_A &e) {
        std::string s = e.what();
        CHECK(s.find("Test exception 237") != std::string::npos);
        CHECK_EQUAL("param", e.param);
        cought = true;
    } catch (const My_derived_exception_B &e) {
        CHECK(false);
    }
    CHECK(cought);
}

TEST(assert_exception)
{
    CHECK_THROW(ASSERT(false), Debug_assert_exception);
}
