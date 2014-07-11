// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Singleton class.
 */

#include <ugcs/vsm/singleton.h>

#include <UnitTest++.h>

using namespace ugcs::vsm;

int creation_count;

class My_class {
public:
    typedef std::shared_ptr<My_class> Ptr;

    int param;

    My_class(): param(0)
    {
        creation_count++;
    }

    My_class(int param): param(param)
    {
        creation_count++;
    }

    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

private:
    static Singleton<My_class> singleton;

};

Singleton<My_class> My_class::singleton;

TEST(basic_functionality)
{
    auto ptr1 = My_class::Get_instance();
    CHECK_EQUAL(1, creation_count);
    CHECK_EQUAL(0, ptr1->param);

    /* Should return the same instance. */
    auto ptr2 = My_class::Get_instance();
    CHECK_EQUAL(1, creation_count);
    CHECK_EQUAL(0, ptr1->param);

    /* Release the first instance and create a new one. */
    ptr1 = nullptr;
    ptr2 = nullptr;
    ptr1 = My_class::Get_instance();
    CHECK_EQUAL(2, creation_count);
    CHECK_EQUAL(0, ptr1->param);

    ptr2 = My_class::Get_instance(1);
    CHECK_EQUAL(2, creation_count);
    CHECK_EQUAL(0, ptr1->param);

    /* Use non-default constructor. */
    ptr1 = nullptr;
    ptr2 = nullptr;
    ptr1 = My_class::Get_instance(2);
    CHECK_EQUAL(3, creation_count);
    CHECK_EQUAL(2, ptr1->param);
}

class My_class_no_def {
public:
    typedef std::shared_ptr<My_class_no_def> Ptr;

    int param;

    My_class_no_def(int param): param(param)
    {
        creation_count++;
    }

    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

private:
    static Singleton<My_class_no_def> singleton;

};

Singleton<My_class_no_def> My_class_no_def::singleton;

TEST(no_default_constructor)
{
    creation_count = 0;
    /* Should return null pointer when no instance yet created. */
    auto ptr1 = My_class_no_def::Get_instance();
    CHECK(!ptr1);
    CHECK_EQUAL(0, creation_count);

    ptr1 = My_class_no_def::Get_instance(10);
    CHECK(ptr1);
    CHECK_EQUAL(1, creation_count);
    CHECK_EQUAL(10, ptr1->param);

    auto ptr2 = My_class_no_def::Get_instance();
    CHECK(ptr2);
    CHECK_EQUAL(1, creation_count);
    CHECK_EQUAL(10, ptr1->param);
}
