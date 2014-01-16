// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/shared_data.h>
#include <vsm/request_worker.h>

#include <UnitTest++.h>

using namespace vsm;
using namespace std;

class Test_case_wrapper
{
public:
    Test_case_wrapper() {
//        vsm::Initialize("vsm.conf", std::ios_base::in);
    }

    ~Test_case_wrapper() {
//        vsm::Terminate();
    }
};

TEST(shared_data_test)
{
    // sync between main and worker
    std::mutex worker_mutex;
    std::condition_variable result_ready_event;
    bool result_ready = false;

    Request_worker::Ptr worker = Request_worker::Create("UT shared data worker");
    worker->Enable();

    // Need timer instance for op waiter timeouts
    Timer_processor::Get_instance()->Enable();

    Shared_data::Acquire_result acq_result(Shared_data::ACQUIRE_RESULT_ERROR);

    auto Wait_for_result = [&]()
        {
            std::unique_lock<std::mutex> lock(worker_mutex);
            result_ready_event.wait(lock, [&](){return result_ready;});
            result_ready = false;
        };

    auto acquire_handler =[&](Shared_data::Acquire_result r, void*)
            {
                LOG("Acquire result=%d", r);
                acq_result = r;
                result_ready = true;
                result_ready_event.notify_all();
            };
    auto acquire_complete = Shared_data::Make_acquire_handler(acquire_handler);

    auto sd1 = Shared_data::Create("vsm_shared_data_test", 100);
    auto sd2 = Shared_data::Create("vsm_shared_data_test", 100);

    // Acquire via first instance.
    LOG("Acquiring 1");
    sd1->Acquire(acquire_complete);
    CHECK(   acq_result == Shared_data::ACQUIRE_RESULT_OK
          || acq_result == Shared_data::ACQUIRE_RESULT_OK_CREATED
          || acq_result == Shared_data::ACQUIRE_RESULT_OK_RECOVERED
          );

    // memory should be acquired now.

    // Try to re-Acquire again the same instance. Must return ACQUIRE_RESULT_ALREADY_ACQUIRED.
    LOG("re-Acquiring 1 with timeout");
    sd1->Acquire(acquire_complete).Timeout(std::chrono::milliseconds(1000));
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_ALREADY_ACQUIRED);

    // Try Acquire again with timeout. Must return ACQUIRE_RESULT_TIMEOUT.
    LOG("Acquiring 2 with timeout");
    sd2->Acquire(acquire_complete).Timeout(std::chrono::milliseconds(100));
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_TIMEOUT);

    // Try Acquire again and cancel the acquire. Must return ACQUIRE_RESULT_CANCELED.
    LOG("Acquiring 2 and cancel");
    auto w = sd2->Acquire(acquire_complete);
    w.Cancel();
    w.Wait();
    Wait_for_result();
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_CANCELED);

    // Release non-acquired data. Must return ACQUIRE_RESULT_RELEASED.
    LOG("Releasing before acquire finished");
    sd2->Acquire(acquire_complete, worker);
    sd2->Release();
    Wait_for_result();
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_RELEASED);

    // Delete object while acquiring. Must return ACQUIRE_RESULT_DESTROYED.
    LOG("Delete object while acquiring");
    sd2->Acquire(acquire_complete, worker);
    sd2 = nullptr;
    Wait_for_result();
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_DESTROYED);

    // Release and acquire again! Must return ACQUIRE_RESULT_OK.
    LOG("Release and acquire again instantly");
    sd1->Release();
    sd1->Acquire(acquire_complete).Timeout(std::chrono::milliseconds(1000));
    CHECK(acq_result == Shared_data::ACQUIRE_RESULT_OK);
    sd1->Release();

    LOG("exiting");

    worker->Disable();
    Timer_processor::Get_instance()->Disable();
}
