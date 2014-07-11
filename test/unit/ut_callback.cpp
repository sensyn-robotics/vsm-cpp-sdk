// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for callback abstraction.
 */

#include <ugcs/vsm/callback.h>
#include <unordered_set>
#include <set>
#include <functional>

#include <UnitTest++.h>

using namespace ugcs::vsm;

int test_arg1;
const char *test_arg2;
int test_arg3;

class Test_data_reseter
{
public:
    Test_data_reseter() {
        Reset_test_data();
    }

    ~Test_data_reseter() {
        Reset_test_data();
    }

private:
    void Reset_test_data()
    {
        test_arg1 = 0;
        test_arg2 = nullptr;
        test_arg3 = 0;
    }

};

typedef enum {
    some_magic_status = 42,
    some_magic_status_bad = 666,
} Callback_status;

class Base {
public:
    Callback_status Foo() { return some_magic_status; };
};

class Derived : public Base {};

void
My_function_void_void() {
    test_arg1 = static_cast<int>(some_magic_status);
}

void
My_function_void_args(int arg1, const char *arg2)
{
    test_arg1 = arg1;
    test_arg2 = arg2;
}

Callback_status
My_function_status_args(int arg1, const char *arg2)
{
    test_arg1 = arg1;
    test_arg2 = arg2;
    return some_magic_status;
}

Callback_status
My_function_status_args_3(int arg1, const char *arg2, int arg3)
{
    test_arg1 = arg1;
    test_arg2 = arg2;
    test_arg3 = arg3;
    return some_magic_status;
}

Callback_status
My_function_status_base(Base* ctx, int arg1, const char *arg2)
{
    test_arg1 = arg1;
    test_arg2 = arg2;
    return ctx->Foo();
}

Callback_status
My_function_status_child(Derived* ctx, int arg1, const char *arg2)
{
    test_arg1 = arg1;
    test_arg2 = arg2;
    return ctx->Foo();
}

/* Different combinations of return types and callback operation arguments,
 * i.e. real uses cases. */

TEST_FIXTURE(Test_data_reseter, function_invocation_void_void)
{
    auto c = Make_callback(My_function_void_void);
    c();
    CHECK_EQUAL(some_magic_status, test_arg1);
}

TEST_FIXTURE(Test_data_reseter, function_invocation_void_args)
{
    Callback_base<void>::Ptr<> c = Make_callback(My_function_void_args, 10, "string");
    c();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST_FIXTURE(Test_data_reseter, function_invocation_status_args)
{
    Callback_base<Callback_status>::Ptr<> c = Make_callback(My_function_status_args, 10, "string");
    CHECK_EQUAL(some_magic_status, c());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST_FIXTURE(Test_data_reseter, function_invocation_status_base)
{
    Base base;
    auto c = Make_callback(My_function_status_base, &base, 10, "string");
    CHECK_EQUAL(some_magic_status, c());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST_FIXTURE(Test_data_reseter, function_invocation_status_base_poly)
{
    /*
     * Note that user provides a function which works with base class only,
     * but callback instantiation provides an instance of some derived class.
     */
    Derived child;
    auto c = Make_callback(My_function_status_base, &child, 10, "string");
    CHECK_EQUAL(some_magic_status, c());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST_FIXTURE(Test_data_reseter, function_invocation_status_child)
{
    Derived child;
    auto c = Make_callback(My_function_status_child, &child, 10, "string");
    CHECK_EQUAL(some_magic_status, c());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST(lambda_invocation)
{
    int test_arg1 = 0;
    const char *test_arg2 = nullptr;

    auto l = [&](int arg1, const char *arg2)
    {
        test_arg1 = arg1;
        test_arg2 = arg2;
    };

    auto h = Make_callback(l, 10, "string");
    h();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST(lambda_invocation_return_type)
{
    int test_arg1 = 0;
    const char *test_arg2 = nullptr;

    auto l = [&](int arg1, const char *arg2)
    {
        test_arg1 = arg1;
        test_arg2 = arg2;
        return Callback_status::some_magic_status;
    };

    auto h = Make_callback(l, 10, "string");
    CHECK_EQUAL(some_magic_status, h());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST(lambda_invocation_polymorphic)
{
    int test_arg1 = 0;
    const char *test_arg2 = nullptr;

    /*
     * Lambda works with base class, but in reality pointer to child object
     * instance is passed.
     */
    auto l = [&](Base* ctx, int arg1, const char *arg2)
    {
        test_arg1 = arg1;
        test_arg2 = arg2;
        return ctx->Foo();
    };

    Derived child;

    auto h = Make_callback(l, &child, 10, "string");
    CHECK_EQUAL(some_magic_status, h());
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST(callable_object_invocation)
{
    class Callable {
    public:
        void
        operator ()(int arg1, const char *arg2)
        {
            test_arg1 = arg1;
            test_arg2 = arg2;
        }
    };

    Callable callable;
    auto h = Make_callback(callable, 10, "string");
    h();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
}

TEST(bound_method_invocation)
{
    class My_class {
    public:
        int test_arg1 = 0;
        const char *test_arg2 = nullptr;

        Callback_status
        Method(Base* ctx, int arg1, const char *arg2)
        {
            test_arg1 = arg1;
            test_arg2 = arg2;
            return ctx->Foo();
        }
    };

    {
        My_class obj;
        Derived derived;
        auto h = Make_callback(&My_class::Method, &obj, &derived, 10, "string");
        CHECK_EQUAL(some_magic_status, h());
        CHECK_EQUAL(10, obj.test_arg1);
        CHECK_EQUAL("string", obj.test_arg2);
    }

    {
        My_class obj;
        Derived derived;
        typedef Callback_status (My_class::*Method_type)(Base *, int, const char *);
        Method_type method_ptr = &My_class::Method;
        auto h = Make_callback(method_ptr, &obj, &derived, 10, "string");
        CHECK_EQUAL(some_magic_status, h());
        CHECK_EQUAL(10, obj.test_arg1);
        CHECK_EQUAL("string", obj.test_arg2);
    }
}

TEST(overridden_virtual_bound_method_invocation)
{
    class My_base {
    public:
        virtual void f() {
            x = static_cast<int>(some_magic_status_bad);
        }
        int x;
    };

    class My_derived: public My_base {
    public:
        virtual void f() override
        {
            x = static_cast<int>(some_magic_status);
        }
    };


    My_derived derived;
    My_base* base = &derived;

    /* Virtual method signature is from Base class, object instance pointer
     * is to base class, actual object is derived -> overridden virtual method
     * from derived class is called. Feel the power? */
    auto h = Make_callback(&My_base::f, base);
    h();
    CHECK_EQUAL(some_magic_status, derived.x);

    std::shared_ptr<My_base> base_ptr = std::make_shared<My_derived>();

    auto h1 = Make_callback(&My_base::f, base_ptr);
    h1();

    CHECK_EQUAL(some_magic_status, base_ptr->x);
}

class My_class {
private:
    void
    My_method_impl(Callback_base<Callback_status>::Ptr<> cbk, int &arg1)
    {
        CHECK_EQUAL(10, arg1);
        arg1 = 20;
        CHECK_EQUAL(some_magic_status, cbk());
    }
public:
    template <class Callback_ptr>
    void
    My_method(Callback_ptr cbk)
    {
        /* Check just the return value */
        Callback_check_type<Callback_ptr, Callback_status>();
        /* ... or check also the first argument */
        Callback_check_type<Callback_ptr, Callback_status, int>();
        /* ... or check both arguments. */
        Callback_check_type<Callback_ptr, Callback_status, int, const char *>();
        My_method_impl(cbk, cbk->template Get_arg<0>());
    }

    int test_arg1 = 0;
    const char *test_arg2 = nullptr;

    Callback_status
    My_method_status_args(int arg1, const char *arg2)
    {
        test_arg1 = arg1;
        test_arg2 = arg2;
        return some_magic_status;
    }
};

DEFINE_CALLBACK_BUILDER(Make_my_callback, (int), (10))

TEST_FIXTURE(Test_data_reseter, type_check)
{
    {
        My_class my_obj;
        auto c = Make_my_callback(My_function_status_args, "string");
        my_obj.My_method(c);
        CHECK_EQUAL(20, test_arg1);
        CHECK_EQUAL("string", test_arg2);
    }

    {
        My_class my_obj;
        auto c = Make_my_callback(&My_class::My_method_status_args, &my_obj, "string");
        my_obj.My_method(c);
        CHECK_EQUAL(20, my_obj.test_arg1);
        CHECK_EQUAL("string", my_obj.test_arg2);
    }
}

TEST(callback_exception)
{
    Callback_base<void>::Ptr<> c;
    CHECK_THROW(c(), Nullptr_exception);
}

TEST(callback_shared_ptr)
{
    auto handler = [](std::shared_ptr<int> int_ptr)
    {
        CHECK_EQUAL(10, *int_ptr);
    };

    auto int_ptr = std::make_shared<int>(10);
    auto cbk = Make_callback(handler, int_ptr);
    CHECK_EQUAL(2, int_ptr.use_count());
    CHECK_EQUAL(10, *int_ptr);
    cbk();
}

void callback_fun_with_enum(Callback_status status __UNUSED)
{

}

TEST(enum_as_callback_arg)
{
    auto cb = Make_callback(callback_fun_with_enum, some_magic_status);
    Callback_check_type<decltype(cb), void, Callback_status>();
    cb();
}

/* Non-copyable class for testing move semantic. */
class NonCopyable {
public:
    int value;

    NonCopyable(int value): value(value) {}

    NonCopyable(const NonCopyable &) = delete;

    NonCopyable(NonCopyable &&src)
    {
        value = src.value;
        src.value = 0;
    }

    void
    My_method_void_nc_args(const NonCopyable &nc1, const NonCopyable &nc2)
    {
        test_arg1 = nc1.value;
        test_arg3 = nc2.value;
    }
};

void
My_function_void_nc_args(const NonCopyable &nc1, const NonCopyable &nc2)
{
    test_arg1 = nc1.value;
    test_arg3 = nc2.value;
}

TEST_FIXTURE(Test_data_reseter, check_move_semantic_basic_func)
{
    /* Callback creation should avoid arguments copying in internal machinery. */

    NonCopyable nc1(10), nc2(20);
    auto cb = Make_callback(My_function_void_nc_args, std::move(nc1), std::move(nc2));
    CHECK_EQUAL(0, nc1.value);
    CHECK_EQUAL(0, nc2.value);
    cb();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, check_move_semantic_basic_method)
{
    NonCopyable nc1(10), nc2(20);
    auto cb = Make_callback(&NonCopyable::My_method_void_nc_args, &nc1,
                            std::move(nc1), std::move(nc2));
    CHECK_EQUAL(0, nc1.value);
    CHECK_EQUAL(0, nc2.value);
    cb();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL(20, test_arg3);
}

template <class Callable, typename... Args>
typename Callback_forced_args<Callable, std::tuple<NonCopyable>, Args...>::Callback_type::Ptr
Make_nc_callback(Callable &&callable, Args &&... args)
{
    NonCopyable nc1(10);
    auto result =  Callback_forced_args<Callable, std::tuple<NonCopyable>, Args...>::Create(
        std::forward<Callable>(callable), std::forward_as_tuple<NonCopyable>(std::move(nc1)),
        std::forward<Args>(args)...);
    CHECK_EQUAL(0, nc1.value);
    return result;
}

TEST_FIXTURE(Test_data_reseter, check_move_semantic_forced_args_func)
{
    NonCopyable nc2(20);
    auto cb = Make_nc_callback(My_function_void_nc_args, std::move(nc2));
    CHECK_EQUAL(0, nc2.value);
    cb();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, check_move_semantic_forced_args_method)
{
    NonCopyable nc2(20);
    auto cb = Make_nc_callback(&NonCopyable::My_method_void_nc_args, &nc2, std::move(nc2));
    CHECK_EQUAL(0, nc2.value);
    cb();
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy)
{
    typedef Callback_proxy<Callback_status, int, const char *> Handler;

    auto My_method = [](Handler handler)
        {
            return handler(10, "string");
        };

    int result = My_method(Make_callback(My_function_status_args_3, 0, "aa", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy_one_arg)
{
    typedef Callback_proxy<Callback_status, int> Handler;

    auto My_method = [](Handler handler)
        {
            return handler(10);
        };

    int result = My_method(Make_callback(My_function_status_args_3, 0, "string", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy_no_args)
{
    typedef Callback_proxy<Callback_status> Handler;

    auto My_method = [](Handler handler)
        {
            return handler();
        };

    int result = My_method(Make_callback(My_function_status_args_3, 10, "string", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy_set_args)
{
    typedef Callback_proxy<Callback_status, int, const char *> Handler;

    auto My_method = [](Handler handler)
        {
            handler.Set_args(10, "string");
            return handler.Invoke();
        };

    int result = My_method(Make_callback(My_function_status_args_3, 0, "aa", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy_copy)
{
    typedef Callback_proxy<Callback_status, int, const char *> Handler;

    Handler unbound_handler;
    CHECK(!unbound_handler);

    auto My_method = [](Handler handler)
        {
            Handler member_handler(handler);
            return member_handler(10, "string");
        };

    int result = My_method(Make_callback(My_function_status_args_3, 0, "aa", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

TEST_FIXTURE(Test_data_reseter, callback_proxy_move)
{
    typedef Callback_proxy<Callback_status, int, const char *> Handler;

    auto My_method = [](Handler handler)
        {
            Handler member_handler(std::move(handler));
            CHECK(!handler);
            return member_handler(10, "string");
        };

    int result = My_method(Make_callback(My_function_status_args_3, 0, "aa", 20));
    CHECK(result == some_magic_status);
    CHECK_EQUAL(10, test_arg1);
    CHECK_EQUAL("string", test_arg2);
    CHECK_EQUAL(20, test_arg3);
}

DEFINE_CALLBACK_BUILDER(Make_shared_ptr_callback, (std::shared_ptr<int>), (nullptr));

TEST(callback_proxy_shared_ptr)
{
    typedef Callback_proxy<void, std::shared_ptr<int>> Handler;

    auto My_callback = [](std::shared_ptr<int> int_ptr)
        {
            CHECK_EQUAL(10, *int_ptr);
        };

    Handler(Make_callback(My_callback, std::make_shared<int>(20)))(std::make_shared<int>(10));

    Handler(Make_callback(My_callback, std::shared_ptr<int>()))(std::make_shared<int>(10));

    Handler(Make_shared_ptr_callback(My_callback))(std::make_shared<int>(10));
}

TEST(callback_proxy_equality_ops)
{
    typedef Callback_proxy<int, int> Handler;

    auto Method = [](int)
        {
            return 42;
        };

    Handler h1;
    Handler h2;
    CHECK(h1 == h2);

    h1 = Make_callback(Method, 5);
    CHECK(h1 != h2);

    h2 = Make_callback(Method, 5);
    /* Different callback instances. */
    CHECK(h1 != h2);

    Handler copy1 = h1;
    CHECK(copy1 == h1);
    CHECK(copy1 != h2);

    auto cbk = Make_callback(Method, 5);
    h1 = cbk;
    h2 = cbk;
    /* The same callback instance. */
    CHECK(h1 == h2);
}

TEST(callback_proxy_container)
{
    typedef Callback_proxy<int, int> Handler;

    auto Method = [](int)
        {
            return 42;
        };

    Handler::Hasher hasher;
    Handler h1;
    Handler h2;
    Handler h3;
    auto cbk = Make_callback(Method, 5);
    h1 = cbk;
    h2 = cbk;
    h3 = Make_callback(Method, 5);

    CHECK_EQUAL(hasher(h1), hasher(h2));

    std::unordered_set<Handler, Handler::Hasher > hmap;
    hmap.insert(h1);
    CHECK_EQUAL(hmap.size(), 1);
    CHECK(hmap.find(h2) != hmap.end());
    CHECK(hmap.find(h3) == hmap.end());

    hmap.insert(h2);
    CHECK(hmap.find(h1) != hmap.end());
    CHECK_EQUAL(hmap.size(), 1);

    hmap.erase(h2);
    CHECK(hmap.find(h2) == hmap.end());
    CHECK_EQUAL(hmap.size(), 0);
}
