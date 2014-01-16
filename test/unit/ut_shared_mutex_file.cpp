// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Unit test for file processor.
 */

#include <vsm/shared_mutex_file.h>
#include <vsm/param_setter.h>
#include <vsm/request_worker.h>

#include <UnitTest++.h>

#include <unistd.h>
#include <fcntl.h>

using namespace vsm;

const char *test_path = "vsm_shared_mutex_test";

/* Fixture class. */
class File_deleter {
public:
    File_deleter()
    {
        int result = access(test_path, F_OK);
        if (result) {
            CHECK_EQUAL(ENOENT, errno);
        } else {
            CHECK_EQUAL(0, unlink(test_path));
        }
    }
};

TEST_FIXTURE(File_deleter, locking_unlocking)
{
    // This is required for Timeout to work.
    Timer_processor::Get_instance()->Enable();
    // This is required for Shared_mutex_file to work.
    File_processor::Get_instance()->Enable();

    {
        auto  m1 = Shared_mutex_file::Create("shared_mutex_test");
        auto  m2 = Shared_mutex_file::Create("shared_mutex_test");

        Io_result result1, result2;

        // First lock must succeed.
        LOG("Test 1. Simple lock. Must succeed.");
        m1->Acquire(Make_setter(result1));
        LOG("Test 1 lock result=%s", Io_stream::Io_result_as_char(result1));
        CHECK(Io_result::OK == result1);

        // Now file1 is holding the lock.

        // Second lock on same handle must fail.
        LOG("Test 2. Try to double lock the same stream. Must error.");
        m1->Acquire(Make_setter(result1));
        LOG("Test 2 lock result=%s", Io_stream::Io_result_as_char(result1));
        CHECK(Io_result::LOCK_ERROR == result1);

        // Lock using second handle to the same file with timeout.
        LOG("Test 3. Lock with timeout while other stream owns the lock. Must timeout.");
        m2->Acquire(Make_setter(result2)).Timeout(std::chrono::milliseconds(100));
        LOG("Test 3 lock result=%s", Io_stream::Io_result_as_char(result2));
        CHECK(Io_result::TIMED_OUT == result2);
    }

    File_processor::Get_instance()->Disable();
    Timer_processor::Get_instance()->Disable();
}

