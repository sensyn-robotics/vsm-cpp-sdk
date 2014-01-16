// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Tests for timer service.
 */

#include <vsm/timer_processor.h>
#include <vsm/request_worker.h>
#include <unordered_set>
#include <vector>

#include <UnitTest++.h>

using namespace vsm;

TEST(timer_usage)
{
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    auto worker = Request_worker::Create("UT timer_usage");
    worker->Enable();

    int count = 3;
    auto handler = [&](int param)
    {
        CHECK_EQUAL(10, param);
        CHECK(count > 0);
        return --count != 0;
    };

    auto timer = timer_proc->Create_timer(std::chrono::milliseconds(100),
                                          Make_callback(handler, 10),
                                          worker);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK_EQUAL(false, timer->Is_running());
    CHECK_EQUAL(0, count);

    worker->Disable();
    timer_proc->Disable();
}

TEST(timer_cancelation)
{
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    auto worker = Request_worker::Create("UT timer_cancelation");
    worker->Enable();

    int count = 0;
    auto handler = [&](int param)
    {
        CHECK_EQUAL(10, param);
        count++;
        return true;
    };

    auto timer = timer_proc->Create_timer(std::chrono::milliseconds(100),
                                          Make_callback(handler, 10),
                                          worker);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    CHECK_EQUAL(true, timer->Is_running());
    CHECK(count > 5);
    timer->Cancel();
    CHECK_EQUAL(false, timer->Is_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(count > 5);
    int initial_count = count;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK_EQUAL(false, timer->Is_running());
    CHECK_EQUAL(initial_count, count);

    worker->Disable();
    timer_proc->Disable();
}

TEST(timer_instant_cancelation)
{
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    auto worker = Request_worker::Create("UT timer_instant_cancelation");
    worker->Enable();

    int count = 0;
    auto handler = [&]()
    {
        count++;
        return true;
    };

    auto timer = timer_proc->Create_timer(std::chrono::milliseconds(100),
                                          Make_callback(handler),
                                          worker);

    timer->Cancel();
    CHECK_EQUAL(false, timer->Is_running());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    CHECK_EQUAL(0, count);

    worker->Disable();
    timer_proc->Disable();
}

TEST(worker_usage)
{
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    Request_worker::Ptr worker = Request_worker::Create("UT timer worker_usage");
    worker->Enable();

    int count = 3;
    auto handler = [&](int param)
    {
        CHECK_EQUAL(10, param);
        CHECK(count > 0);
        return --count != 0;
    };

    auto timer = timer_proc->Create_timer(std::chrono::milliseconds(100),
                                          Make_callback(handler, 10),
                                          worker);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    CHECK_EQUAL(false, timer->Is_running());
    CHECK_EQUAL(0, count);

    worker->Disable();
    timer_proc->Disable();
}

TEST(cancel_timer_before_termination)
{
    constexpr size_t NUM_OF_TIMERS = 1000;
    Timer_processor::Timer::Ptr timers[NUM_OF_TIMERS];
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    auto worker = Request_worker::Create("UT cancel_timer_before_termination");
    worker->Enable();

    int count = 0;

    /* Create endless timers. */
    for (size_t i = 0; i < NUM_OF_TIMERS; i++) {
        auto handler = [&]() { count++; return true; };
        timers[i] = timer_proc->Create_timer(
                /* Some random spread. */
                std::chrono::milliseconds((std::rand() % 20) + 1),
                Make_callback(handler),
                worker);
    }
    /* Let them run for some time. */
    std::this_thread::sleep_for(std::chrono::seconds(2));
    /* Simple sanity. */
    CHECK(count > 1000);

    auto canceller = [&](Request::Ptr request)
    {
        for (size_t i = 0; i < NUM_OF_TIMERS; i++) {
            timers[i]->Cancel();
        }
        request->Complete();
    };

    /* Cancel all timers in a worker thread. */
    auto req = Request::Create();
    req->Set_processing_handler(
            Make_callback(canceller, req));
    worker->Submit_request(req);
    req->Wait_done(false);

    /* Disable worker and processor. */
    worker->Disable();
    timer_proc->Disable();
}

TEST(cancel_in_handler)
{
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    auto worker = Request_worker::Create("UT timer cancel_in_handler");
    worker->Enable();

    Timer_processor::Timer::Ptr timer;
    auto handler = [&]()
        {
            timer->Cancel();
            timer = nullptr;
            return true;
        };
    timer = timer_proc->Create_timer(
            std::chrono::milliseconds(100),
            Make_callback(handler),
            worker);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    worker->Disable();
    timer_proc->Disable();
}

/* This test tries to reproduce the race condition situation when timer B is
 * attached to timer A, and timer A is cancelled immediately after the creation.
 * Expected result is that timer B should still be executed.
 */
TEST(cancel_with_attached_timers)
{
    constexpr int NUM_OF_ATTEMPTS = 1000;
    Timer_processor::Ptr timer_proc = Timer_processor::Create();
    timer_proc->Enable();

    std::unordered_set<Timer_processor::Timer::Ptr> created_timers;

    Request_worker::Ptr worker = Request_worker::Create("UT cancel_with_attached_timers");
    worker->Enable();

    int called = 0;

    auto mandatory_handler = [&]()
    {
        called++;
        return false;
    };

    auto optional_handler = [&]()
    {
        return false;
    };

    for (auto i = 0; i < NUM_OF_ATTEMPTS; i++) {
        auto timer1 = timer_proc->Create_timer(
                std::chrono::milliseconds(50),
                Make_callback(optional_handler),
                worker);
        created_timers.insert(timer1);
        /* Introduce some random behavior. */
        int timeout = 49;
        if (std::rand() & 0x1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            /* Compensate the delay a little bit. */
            timeout = 47;
        }
        /* Simulate race condition. timer1 is tried to be cancelled while running. */
        timer1->Cancel();
        auto timer2 = timer_proc->Create_timer(
                std::chrono::milliseconds(timeout),
                Make_callback(mandatory_handler),
                worker);
        created_timers.insert(timer2);
    }

    /* Give some time for timers to finish. Not very robust. */
    std::this_thread::sleep_for(std::chrono::seconds(3));

    CHECK_EQUAL(NUM_OF_ATTEMPTS, called);

    /* Make sure, all timers are cancelled whatsoever. */
    for(auto& iter: created_timers) {
        iter->Cancel();
    }

    worker->Disable();
    timer_proc->Disable();
}

TEST(cancel_race)
{

    int repeat = 30;
    while (repeat--) {
        volatile bool terminate = false;
        std::mutex mutex;
        constexpr int NUM_OF_TIMERS = 1000;
        std::vector<Timer_processor::Timer::Ptr> timers(NUM_OF_TIMERS);
        Timer_processor::Ptr timer_proc = Timer_processor::Create();
        timer_proc->Enable();
        std::unordered_set<Timer_processor::Timer::Ptr> created_timers;

        Request_worker::Ptr worker = Request_worker::Create("UT cancel race");
        worker->Enable();

        auto handler = []()
        {
            /* Don't recreate timer, otherwise there is a race between
             * cancel for the old and re-created timer.
             */
            return false;
        };

        auto creator_thread = [&]()
        {
            while (!terminate) {
                auto idx = std::rand() % NUM_OF_TIMERS;
                std::unique_lock<std::mutex> lock(mutex);
                if (!timers[idx]) {
                    auto timer = timer_proc->Create_timer(
                            std::chrono::milliseconds(std::rand() % 3),
                            Make_callback(handler),
                            worker);
                    timers[idx] = timer;
                } else {
                    timers[idx]->Cancel();
                    timers[idx] = nullptr;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 1));
            }
        };

        std::thread t1(creator_thread);
        std::thread t2(creator_thread);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        terminate = true;
        t1.join();
        t2.join();
        for (auto& iter: timers) {
            if (iter) {
                iter->Cancel();
            }
        }
        worker->Disable();
        timer_proc->Disable();
        LOG_INFO("Iteration done.");
    }
}
