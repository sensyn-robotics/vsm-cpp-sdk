// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for Operation_waiter class.
 */

#include <ugcs/vsm/vsm.h>
#include <ugcs/vsm/operation_waiter.h>
#include <ugcs/vsm/request_temp_completion_context.h>
#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/param_setter.h>

#include <thread>

#include <UnitTest++.h>

using namespace ugcs::vsm;

class Fixture {
public:
    Fixture()
    {
        Initialize("vsm.conf", std::ios_base::in);
    }

    ~Fixture()
    {
        Terminate();
    }
};

/* Typical simple processor with dedicated thread. */
class Some_processor: public Request_processor {
    DEFINE_COMMON_CLASS(Some_processor, Request_container)
public:
    Some_processor() : Request_processor("UT some processor")
    {}

    /* Typical asynchronous operation method. */
    template <class Callback_ptr>
    Operation_waiter
    Some_method(int param, Callback_ptr handler = Callback_base<void>::Ptr<>(),
                Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        Callback_check_type<Callback_ptr, void, int>();
        return Some_method_impl(param, handler->template Get_arg<0>(), handler,
                                comp_ctx);
    }

    /* Typical asynchronous operation method with long processing. */
    template <class Callback_ptr>
    Operation_waiter
    Some_long_method(int param, std::chrono::milliseconds delay,
                     Callback_ptr handler = Callback_base<void>::Ptr<>(),
                     Request_completion_context::Ptr comp_ctx = Request_temp_completion_context::Create())
    {
        Callback_check_type<Callback_ptr, void, int>();
        return Some_long_method_impl(param, delay, handler->template Get_arg<0>(),
                                     handler, comp_ctx);
    }
private:
    /* Dedicated thread. */
    std::thread thread;
    /* Timer for long method execution. */
    Timer_processor::Timer::Ptr long_method_timer;

    /* Worker for timer completion handlers. */
    Request_worker::Ptr worker = Request_worker::Create("UT OP waiter timer worker");

    virtual void
    On_enable() override
    {
        Request_processor::On_enable();
        worker->Enable();
        thread = std::thread(&Some_processor::Processing_loop, Shared_from_this());
    }

    virtual void
    On_disable() override
    {
        Set_disabled();
        /* Wait for dedicated thread terminates. */
        thread.join();
        if (long_method_timer) {
            long_method_timer->Cancel();
        }
        worker->Disable();
    }

    /* Typical asynchronous operation method. */
    Operation_waiter
    Some_method_impl(int param, int &result, Request::Handler handler,
                     Request_completion_context::Ptr comp_ctx)
    {
        Request::Ptr request = Request::Create();
        auto proc_handler = [&result, param, request]()
            {
                result = param;
                request->Complete();
            };
        request->Set_processing_handler(Make_callback(proc_handler));
        request->Set_completion_handler(comp_ctx, handler);
        Submit_request(request);
        return request;
    }

    /* Typical long asynchronous operation method. */
    Operation_waiter
    Some_long_method_impl(int param, std::chrono::milliseconds delay,
                          int &result, Request::Handler handler,
                          Request_completion_context::Ptr comp_ctx)
    {
        Request::Ptr request = Request::Create();

        auto proc_end_handler = [this, &result, param, request]()
            {
                result = param;
                request->Complete();
                long_method_timer = nullptr;
                return false;
            };

        auto proc_start_handler = [this, delay, proc_end_handler]()
            {
                if (long_method_timer) {
                    long_method_timer->Cancel();
                }
                long_method_timer = Timer_processor::Get_instance()->
                    Create_timer(delay, Make_callback(proc_end_handler), worker);
            };

        auto cancellation_hanler = [this, request]()
            {
                long_method_timer->Cancel();
                request->Abort();
            };

        request->Set_processing_handler(Make_callback(proc_start_handler));
        request->Set_completion_handler(comp_ctx, handler);
        request->Set_cancellation_handler(Make_callback(cancellation_hanler));
        Submit_request(request);
        return request;
    }
};

DEFINE_CALLBACK_BUILDER(Make_some_callback, (int), (10))

TEST(simple_async)
{
    Some_processor::Ptr proc = Some_processor::Create();

    proc->Enable();

    int test_result = 0;
    /* Initiate asynchronous operation. */
    auto waiter = proc->Some_method(10, Make_setter(test_result));

    /* Do some unrelated stuff here. */
    /* ... */

    /* Synchronize when result is needed. */
    waiter.Wait();
    CHECK_EQUAL(10, test_result);

    proc->Disable();
}

/* Abort discards completed request, cancel does not. */
TEST(async_abort_cancel)
{
    Some_processor::Ptr proc = Some_processor::Create();

    proc->Enable();

    int test_result = 0;
    /* Initiate asynchronous operation. */
    auto waiter = proc->Some_method(42, Make_setter(test_result));

    /* Give some time for request to complete. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    waiter.Cancel();

    /* Synchronize when result is needed. */
    waiter.Wait();
    /* Completion handler is called, because Cancel was called after
     * request is completed. */
    CHECK_EQUAL(42, test_result);

    int test_result1 = 0;
    int test_result2 = 0;

    /* Initiate asynchronous operation. */
    waiter = proc->Some_method(43, Make_setter(test_result1));
    CHECK_EQUAL(0, test_result1);

    /* Initiate another, without explicitly waiting for previous to complete and
     * assign result to the same (non empty!) waiter. */
    waiter = proc->Some_method(44, Make_setter(test_result2));
    /* Old waiter from test_result1 is now overwritten by move operation,
     * so it should be completed, but not the test_result2.
     */
    CHECK_EQUAL(43, test_result1);
    CHECK_EQUAL(0, test_result2);
    /* Let test_result2 to complete. */
    waiter.Wait();
    CHECK_EQUAL(44, test_result2);

    test_result = 0;
    /* Initiate asynchronous operation. */
    waiter = proc->Some_method(45, Make_setter(test_result));

    /* Give some time for request to complete. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    waiter.Abort();

    /* Synchronize when result is needed. */
    waiter.Wait();
    /* Completion handler is not called, because Abort discards the request even
     * if it was called after request is completed. */
    CHECK_EQUAL(0, test_result);

    /* Make sure, wait can be called again. */
    waiter.Wait();

    proc->Disable();
}

TEST(simplest_async)
{
    Some_processor::Ptr proc = Some_processor::Create();

    proc->Enable();

    int test_result = 0;

    /* Just do it synchronously. */
    proc->Some_method(10, Make_setter(test_result));
    CHECK_EQUAL(10, test_result);

    /* ... the same as */
    proc->Some_method(20, Make_setter(test_result)).Wait();
    CHECK_EQUAL(20, test_result);

    proc->Disable();
}

TEST(worker)
{
    Some_processor::Ptr proc = Some_processor::Create();
    proc->Enable();
    Request_worker::Ptr worker = Request_worker::Create("UT OP worker");
    worker->Enable();

    int test_result = 0;

    /* Let the worker process the completion, pass "false" to wait. */
    proc->Some_method(10, Make_setter(test_result), worker).Wait(false);
    CHECK_EQUAL(10, test_result);

    /* No matter who will process the completion. */
    proc->Some_method(20, Make_setter(test_result), worker).Wait();
    CHECK_EQUAL(20, test_result);

    /* It will be processed without synchronization too. */
    proc->Some_method(30, Make_setter(test_result), worker);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK_EQUAL(30, test_result);

    worker->Disable();
    proc->Disable();
}

TEST(wait_timeout)
{
    Some_processor::Ptr proc = Some_processor::Create();
    proc->Enable();
    Request_worker::Ptr worker = Request_worker::Create("UT OP wait_timeout");
    worker->Enable();

    int test_param = 0;
    auto handler = [&test_param](int param)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            test_param = param;
        };

    /* Let the worker process the completion, pass "false" to wait. */
    auto waiter = proc->Some_method(20, Make_some_callback(handler), worker);
    bool is_processed = waiter.Wait(false, std::chrono::milliseconds(100));
    CHECK_EQUAL(false, is_processed);
    CHECK_EQUAL(0, test_param);
    is_processed = waiter.Wait(false, std::chrono::seconds(1));
    CHECK_EQUAL(true, is_processed);
    CHECK_EQUAL(20, test_param);

    worker->Disable();
    proc->Disable();
}

TEST_FIXTURE(Fixture, timeout)
{
    Some_processor::Ptr proc = Some_processor::Create();
    proc->Enable();
    Request_worker::Ptr worker = Request_worker::Create("UT OP timeout");
    worker->Enable();

    int test_param = 0;
    auto handler = [&test_param](int param)
        {
            test_param = param;
        };

    bool timeout_called = false;
    auto timeout_handler = [&timeout_called](const Operation_waiter::Ptr &waiter)
        {
            timeout_called = true;
            CHECK_EQUAL(false, waiter->Is_done());
            waiter->Cancel();
        };


    /* Without handling, just canceling. */
    test_param = 0;
    auto waiter = proc->Some_long_method(20, std::chrono::milliseconds(1000),
                                         Make_some_callback(handler), worker);
    waiter.Timeout(std::chrono::milliseconds(500));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK_EQUAL(false, waiter.Is_done());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK_EQUAL(true, waiter.Is_done());
    CHECK_EQUAL(0, test_param);
    if (!waiter.Is_done()) {
        /* Diagnostic. Try to wait even more. */
        LOG_INFO("Waiting more... (1)");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        CHECK_EQUAL(true, waiter.Is_done());
    }


    /* Handle timeout. */
    waiter = proc->Some_long_method(20, std::chrono::milliseconds(1000),
                                    Make_some_callback(handler), worker);
    waiter.Timeout(std::chrono::milliseconds(500),
                   Make_timeout_callback(timeout_handler), false);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK_EQUAL(false, waiter.Is_done());
    CHECK_EQUAL(false, timeout_called);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK_EQUAL(true, waiter.Is_done());
    CHECK_EQUAL(true, timeout_called);
    if (!waiter.Is_done()) {
        /* Diagnostics. Try to wait even more. */
        LOG_INFO("Waiting more... (2)");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        CHECK_EQUAL(true, waiter.Is_done());
        CHECK_EQUAL(true, timeout_called);
    }
    CHECK_EQUAL(0, test_param);


    /* Timeout not reached. */
    timeout_called = false;
    waiter = proc->Some_long_method(20, std::chrono::milliseconds(500),
                                    Make_some_callback(handler), worker);
    waiter.Timeout(std::chrono::milliseconds(1000),
                   Make_timeout_callback(timeout_handler), false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK_EQUAL(false, waiter.Is_done());
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    CHECK_EQUAL(true, waiter.Is_done());
    CHECK_EQUAL(false, timeout_called);
    CHECK_EQUAL(20, test_param);


    worker->Disable();
    proc->Disable();
}
