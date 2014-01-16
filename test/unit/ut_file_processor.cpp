// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Unit test for file processor.
 */

#include <vsm/file_processor.h>
#include <vsm/param_setter.h>
#include <vsm/request_worker.h>

#include <UnitTest++.h>

#include <unistd.h>
#include <fcntl.h>

using namespace vsm;

const char *test_path = "vsm_file_processor_test";

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

TEST_FIXTURE(File_deleter, basic_functionality)
{
    File_processor::Ptr proc = File_processor::Create();
    proc->Enable();

    {

    /* Invalid modes should not be accepted. */
    CHECK_THROW(proc->Open(test_path, ""), Invalid_param_exception);
    CHECK_THROW(proc->Open(test_path, "a"), Invalid_param_exception);
    CHECK_THROW(proc->Open(test_path, "rw"), Invalid_param_exception);
    CHECK_THROW(proc->Open(test_path, "r+xx"), Invalid_param_exception);
    CHECK_THROW(proc->Open(test_path, "w+xx"), Invalid_param_exception);


    /* Should throw exception when file does not exist and mode requires existence. */
    CHECK_THROW(proc->Open(test_path, "r"), File_processor::Not_found_exception);


    auto file = proc->Open(test_path, "w");
    CHECK_EQUAL(test_path, file->Get_name());
    file->Write(Io_buffer::Create("test string\n"));
    file->Write(Io_buffer::Create("line 2\n"));


    /* Read should fail because the file is opened with write access only. */
    file->Seek(0);
    Io_buffer::Ptr buf;
    Io_result result;
    file->Read(sizeof("test string") - 1, sizeof("test string") - 1,
               Make_setter(buf, result));
    CHECK(result == Io_result::PERMISSION_DENIED);


    file->Close().Wait();
    result = Io_result::OK;
    file->Write(Io_buffer::Create("test"), Make_setter(result));
    CHECK(result == Io_result::CLOSED);
//    CHECK_THROW(file->Write(Io_buffer::Create("test")),
//                File_processor::Closed_stream_exception);


    file = proc->Open(test_path, "r");
    file->Read(sizeof("test string") - 1, sizeof("test string") - 1,
               Make_setter(buf, result));
    CHECK(result == Io_result::OK);
    CHECK_EQUAL(sizeof("test string") - 1, buf->Get_length());
    std::string s1(reinterpret_cast<const char *>(buf->Get_data()), buf->Get_length());
    CHECK_EQUAL("test string", s1);


    /* Write should fail because the file is opened with read access only. */
    file->Write(Io_buffer::Create("test string\n"), Make_setter(result));
    CHECK(result == Io_result::PERMISSION_DENIED);


    file = proc->Open(test_path, "r+");
    file->Read(sizeof("test string") - 1, sizeof("test string") - 1,
               Make_setter(buf, result));
    CHECK(result == Io_result::OK);
    CHECK_EQUAL(sizeof("test string") - 1, buf->Get_length());
    std::string s2(reinterpret_cast<const char *>(buf->Get_data()), buf->Get_length());
    CHECK_EQUAL("test string", s2);

    file->Seek(5);
    file->Write(Io_buffer::Create("012345"));
    file->Seek(5);
    file->Read(6, 6, Make_setter(buf, result));
    CHECK(result == Io_result::OK);
    CHECK_EQUAL(6, buf->Get_length());
    std::string s3(reinterpret_cast<const char *>(buf->Get_data()), buf->Get_length());
    CHECK_EQUAL("012345", s3);


    /* Cannot reliably check cancellation - operation may or may not be already
     * completed or queued when Cancel() is called.
     */
    file->Seek(5);
    file->Write(Io_buffer::Create("012345")).Cancel();


    /* Should throw exception when file exists and mode requires new file creation. */
    CHECK_THROW(proc->Open(test_path, "wx"), File_processor::Already_exists_exception);
    CHECK_THROW(proc->Open(test_path, "w+x"), File_processor::Already_exists_exception);

    }

    proc->Disable();
}

TEST_FIXTURE(File_deleter, end_of_file)
{
    File_processor::Ptr proc = File_processor::Create();
    proc->Enable();

    {
    auto file = proc->Open(test_path, "w");
    file = proc->Open(test_path, "r");
    Io_buffer::Ptr buf;
    Io_result result;
    file->Read(16, 1, Make_setter(buf, result));
    CHECK_EQUAL(0, buf->Get_length());
    CHECK(Io_result::END_OF_FILE == result);

    file = proc->Open(test_path, "w");
    file->Write(Io_buffer::Create("0123"));
    file = proc->Open(test_path, "r");
    file->Read(16, 16, Make_setter(buf, result));
    CHECK_EQUAL(4, buf->Get_length());
    CHECK(Io_result::END_OF_FILE == result);

    file = proc->Open(test_path, "r");
    file->Read(16, 1, Make_setter(buf, result));
    CHECK_EQUAL(4, buf->Get_length());
    CHECK(Io_result::OK == result);
    }

    proc->Disable();
}

TEST_FIXTURE(File_deleter, file_locking)
{
    File_processor::Ptr proc = File_processor::Create();
    // This is required for Timeout to work.
    Timer_processor::Get_instance()->Enable();
    proc->Enable();

    {
    auto file1 = proc->Open(test_path, "rx");
    auto file2 = proc->Open(test_path, "rx");
    Io_result result1, result2;

    // First lock must succeed.
    LOG("Test 1. Simple lock. Must succeed.");
    file1->Lock(Make_setter(result1));
    LOG("Test 1 lock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::OK == result1);

    // Now file1 is holding the lock.

    // Second lock on same handle must fail.
    LOG("Test 2. Try to double lock the same stream. Must error.");
    file1->Lock(Make_setter(result1));
    LOG("Test 2 lock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::LOCK_ERROR == result1);

    // Lock using second handle to the same file with timeout.
    LOG("Test 3. Lock with timeout while other stream owns the lock. Must timeout.");
    file2->Lock(Make_setter(result2)).Timeout(std::chrono::milliseconds(100));
    LOG("Test 3 lock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::TIMED_OUT == result2);

    // Lock using second handle to the same file and cancel.
    LOG("Test 4. Initiate Lock and cancel while other stream owns the lock."
        "Must return canceled.");
    auto waiter = file2->Lock(Make_setter(result2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    waiter.Cancel();
    waiter.Wait();
    LOG("Test 4 lock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::CANCELED == result2);

    // Lock using second handle to the same file should not return.
    LOG("Test 5. Initiate Lock and close other stream. Must succeed.");
    waiter = file2->Lock(Make_setter(result2));
    // File close should unblock the pending Lock via file2.
    file1->Close();
    waiter.Wait();
    LOG("Test 5 lock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::OK == result2);

    // Now file2 is holding the lock.
    // reopen the file and try to lock.
    LOG("Test 6. Initiate Lock and Unlock while lock pending."
        "Unlock must succeed, Lock must return with error.");
    file1 = proc->Open(test_path, "rx");
    waiter = file1->Lock(Make_setter(result1));
    // Unlock while lock is pending. Should return OK.
    file1->Unlock(Make_setter(result2));
    LOG("Test 6 unlock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::OK == result2);
    // Pending lock should fail.
    waiter.Wait();
    LOG("Test 6 lock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::LOCK_ERROR == result1);

    // Try to reacquire the lock.
    LOG("Test 7. Lock and Unlock via other stream. Must succeed.");
    waiter = file1->Lock(Make_setter(result1));

    // this should allow file1->Lock() to succeed.
    file2->Unlock(Make_setter(result2));
    LOG("Test 7 unlock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::OK == result2);
    waiter.Wait();
    LOG("Test 7 lock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::OK == result1);

    // Now file1 is holding the lock.
    // Close while lock pending. Must return error.
    LOG("Test 8. Pend lock and close. Must return CLOSED.");
    waiter = file2->Lock(Make_setter(result2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    file2->Close();
    waiter.Wait();
    LOG("Test 8 lock result=%s", Io_stream::Io_result_as_char(result2));
    CHECK(Io_result::CLOSED == result2);

    // Unlock file1. Must succeed.
    LOG("Test 9. Unlock the locked stream. Must succeed.");
    file1->Unlock(Make_setter(result1));
    LOG("Test 9 unlock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::OK == result1);

    // Unlock file1. Must error.
    LOG("Test 10. Unlock the non-locked stream. Must error.");
    file1->Unlock(Make_setter(result1));
    LOG("Test 10 unlock result=%s", Io_stream::Io_result_as_char(result1));
    CHECK(Io_result::LOCK_ERROR == result1);
    }

    Timer_processor::Get_instance()->Disable();
    proc->Disable();
}

