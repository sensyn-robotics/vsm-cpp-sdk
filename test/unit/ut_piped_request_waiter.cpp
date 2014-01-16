// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.


#include <vsm/piped_request_waiter.h>

#include <iostream>
#include <thread>
#include <array>
#include <initializer_list>

#include <UnitTest++.h>

using namespace vsm;

//XXX
#ifdef __unix__

TEST(test_wait_notify_single_thread)
{
    UNITTEST_TIME_CONSTRAINT(10000); /* 10 seconds */

    Piped_request_waiter waiter;
    auto t1 = std::chrono::steady_clock::now();
    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(2500)));
    auto t2 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout<<"Empty wait requested "<<2500<<std::endl;
    std::cout<<"Empty wait elapsed "<<elapsed<<std::endl;
    CHECK(elapsed >= 2500);

    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(0)));

    waiter.Notify();
    CHECK_EQUAL(true, waiter.Wait());
    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(0)));
    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(10)));

    waiter.Notify();
    waiter.Notify();
    waiter.Notify();
    CHECK_EQUAL(true, waiter.Wait(std::chrono::seconds(11))); /* Should return immediately */
    CHECK_EQUAL(false, waiter.Wait(std::chrono::seconds(0)));
    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(0)));
    CHECK_EQUAL(false, waiter.Wait(std::chrono::milliseconds(10)));

    for (int i = 0; i < 1000; i++) {
        waiter.Notify();
    }
    CHECK_EQUAL(true, waiter.Wait());
    CHECK_EQUAL(false, waiter.Wait(std::chrono::seconds(0)));
}

TEST(test_wait_notify_multiple_threads_with_sleeps)
{
    UNITTEST_TIME_CONSTRAINT(60000); /* 60 seconds */

    Piped_request_waiter waiter;
    std::atomic_int work_items(0);
    int processed = 0;
    const int num_of_generators = 16;
    const int work_per_generator = 1000;

    auto generator = [&] () {
        for (int i = 0; i < work_per_generator; i++) {
            work_items += 1;
            waiter.Notify();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    auto receiver = [&] () {
        while(processed < num_of_generators * work_per_generator) {
            CHECK(waiter.Wait(std::chrono::seconds(61)));
            processed += work_items.exchange(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };


    std::array<std::unique_ptr<std::thread>, num_of_generators> generators;

    for (auto & t : generators) {
        t.reset(new std::thread(generator));
    }

    std::thread r(receiver);

    for (auto &t : generators) {
        t->join();
    }
    r.join();
    CHECK_EQUAL(num_of_generators * work_per_generator, processed);
}

TEST(test_wait_notify_multiple_threads_tight)
{
    UNITTEST_TIME_CONSTRAINT(60000); /* 60 seconds */

    Piped_request_waiter waiter;
    std::atomic_int work_items(0);
    int processed = 0;
    const int num_of_generators = 32;
    const int work_per_generator = 65535;

    auto generator = [&] () {
        for (int i = 0; i < work_per_generator; i++) {
            work_items += 1;
            waiter.Notify();
        }
    };

    auto receiver = [&] () {
        while(processed < num_of_generators * work_per_generator) {
            CHECK(waiter.Wait(std::chrono::seconds(61)));
            processed += work_items.exchange(0);
        }
    };


    std::array<std::unique_ptr<std::thread>, num_of_generators> generators;

    for (auto & t : generators) {
        t.reset(new std::thread(generator));
    }

    std::thread r(receiver);

    for (auto &t : generators) {
        t->join();
    }
    r.join();
    CHECK_EQUAL(num_of_generators * work_per_generator, processed);
}

#endif /* __unix__ */ //XXX
