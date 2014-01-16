// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_data.cpp
 *
 *  Created on: Oct 24, 2013
 *      Author: Janis
 */

#include <vsm/shared_data.h>
namespace vsm
{

constexpr int Shared_data::METADATA_TIMEOUT_MS;
constexpr std::chrono::milliseconds Shared_data::METADATA_TIMEOUT;
constexpr std::chrono::milliseconds Shared_data::HEARTBEAT_TIMEOUT;
constexpr int Shared_data::METADATA_TIMEOUT_COUNT_MAX;
constexpr int Shared_data::METADATA_VERSION;
constexpr int Shared_data::MAX_SIM_CLIENTS;

#define SLOG(...) //LOG(__VA_ARGS__)

Shared_data::Shared_data(const std::string& name, const size_t size, const size_t align)
:name(name), size(size), align(align), my_client_id(0), my_metadata(nullptr)
{
    SLOG("%d - constructor called, %s", my_client_id, name.c_str());
    master_locker = Shared_semaphore::Create();
    data_gate = Shared_semaphore::Create();
    shared_memory = Shared_memory::Create();
    thread = std::thread(&Shared_data::Main_loop, this);
}

Shared_data::~Shared_data()
{
    SLOG("%d - ~destructor called, %s", my_client_id, name.c_str());
    // release the data without change in state.
    std::unique_lock<std::mutex> lk(thread_mutex);
    // notify thread to stop execution.
    event_stop = true;
    tread_event.notify_all();
    if (std::this_thread::get_id() != thread.get_id() && thread.joinable()) {
        lk.unlock();
        thread.join();
    }
}

void
Shared_data::Initialize_metadata()
{
    size_t meta_data_size = sizeof(Shared_metadata);
    meta_data_size += (align - (meta_data_size % align)) % align;

    auto memory_open_result = shared_memory->Open(name + "_memory", meta_data_size + size);

    if (memory_open_result == Shared_memory::OPEN_RESULT_ERROR) {
        VSM_EXCEPTION(Exception, "Failed to open shared memory!");
    }

    if (data_gate->Open(name + "_data_gate", 1, 1) == Shared_semaphore::OPEN_RESULT_ERROR) {
        VSM_EXCEPTION(Exception, "Failed to open data_gate semaphore!");
    }

    my_metadata = reinterpret_cast<Shared_metadata*>(shared_memory->Get());

    my_data = reinterpret_cast<char*>(shared_memory->Get()) + meta_data_size;

    if (memory_open_result == Shared_memory::OPEN_RESULT_CREATED) {
        //TODO: delete all existing semaphores here.
        // To handle case when somebody deleted shared memory but
        // did not delete the semaphores. can happen on linux only.
        my_metadata->next_client_id = 1;
        my_metadata->version = METADATA_VERSION;
        // Assuming that memory fence acts as expected on shared memory as well.
        std::atomic_thread_fence(std::memory_order_release);
    } else {
        for (auto i = 0; i < 1000; i++) {
            // Assuming that memory fence acts as expected on shared memory as well.
            std::atomic_thread_fence(std::memory_order_acquire);
            if (my_metadata->version != 0)
                break;
            LOG("Waiting for shared memory metadata to initialize...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };
    }

    if (my_metadata->version != METADATA_VERSION) {
        VSM_EXCEPTION(Exception, "Shared data present but invalid version: %d!", my_metadata->version);
    }
}

Shared_data::Acquire_result
Shared_data::Master_lock()
{
    auto ret = ACQUIRE_RESULT_OK;
    auto wait_res = master_locker->Wait(METADATA_TIMEOUT);
    if (wait_res == Shared_semaphore::WAIT_RESULT_ERROR) {

        // Initialize metadata
        Initialize_metadata();

        // Create master_locker
        master_lock_id = my_metadata->semaphore_id;

        auto sem_open_result = master_locker->Open(
                name + "_master_lock_" + std::to_string(master_lock_id), 0, 1);
        if (sem_open_result == Shared_semaphore::OPEN_RESULT_CREATED){
            if (my_metadata->current_client_id) {
                LOG("Metadata was locked by client=%d", my_metadata->current_client_id);
            }
            my_metadata->semaphore_id = master_lock_id;
            my_metadata->client_count = 0;
            my_metadata->current_client_id = 0;
            wait_res = Shared_semaphore::WAIT_RESULT_OK;
        } else if (sem_open_result == Shared_semaphore::OPEN_RESULT_OK) {
            wait_res = master_locker->Wait(METADATA_TIMEOUT);
        } else {
            VSM_EXCEPTION(Exception, "Failed to open master semaphore");
        }
    }

    while (wait_res == Shared_semaphore::WAIT_RESULT_TIMEOUT) {
        LOG("Failed to lock metadata. Recreating master_locker...");

        master_lock_id++;

        auto sem_open_result = master_locker->Open(
                name + "_master_lock_" + std::to_string(master_lock_id), 0, 1);
        if (sem_open_result == Shared_semaphore::OPEN_RESULT_CREATED){
            if (my_metadata->current_client_id) {
                LOG("Metadata was locked by client=%d", my_metadata->current_client_id);
            }
            my_metadata->semaphore_id = master_lock_id;
            my_metadata->client_count = 0;
            my_metadata->current_client_id = 0;
            my_metadata->data_state = DATA_STATE_RECOVERED;
            wait_res = Shared_semaphore::WAIT_RESULT_OK;
        } else if (sem_open_result == Shared_semaphore::OPEN_RESULT_OK) {
            wait_res = master_locker->Wait(METADATA_TIMEOUT);
        } else {
            VSM_EXCEPTION(Exception, "Failed to create master lock");
        }
        ret = ACQUIRE_RESULT_OK_RECOVERED;
    }

    if (wait_res == Shared_semaphore::WAIT_RESULT_ERROR) {
        VSM_EXCEPTION(Exception, "Failed to acquire Metadata lock!");
    }

    if (my_metadata->current_client_id != 0) {
        VSM_EXCEPTION(Exception, "Metadata inconsistent cur_client=%d", my_metadata->current_client_id);
    }

    if (my_client_id == 0) {
        my_client_id = my_metadata->next_client_id++;
        SLOG("%d - myclient_id", my_client_id);
    }
    ASSERT(my_metadata->semaphore_id == master_lock_id);
    my_metadata->current_client_id = my_client_id;
    SLOG("%d - v--- count=%d ------- Master LOCKED", my_client_id, my_metadata->client_count);
//    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    thread_mutex.lock();
    return ret;
}

void
Shared_data::Dump_client_list(int)
{
    return;
    for (uint32_t i = 0; i < my_metadata->client_count; i++) {
//        auto &c = my_metadata->clients[i];
//        SLOG("%d cid=%d, time=%lu", i, c.client_id, c.last_heartbeat_time.time_since_epoch().count());
    }
}

// Master_lock must be held when running this function.
void
Shared_data::Remove_client_from_list(uint32_t idx)
{
    for (uint32_t j = idx + 1; j < my_metadata->client_count; j++) {
        my_metadata->clients[j - 1] = my_metadata->clients[j];
    }
    my_metadata->client_count--;
}

bool
Shared_data::Insert_client_in_list()
{
    auto cur_time = std::chrono::steady_clock::now();
    auto found_myself = false;
    for (uint32_t i = 0; i < my_metadata->client_count; i++) {
        if (my_metadata->clients[i].client_id == my_client_id) {
            found_myself = true;
        } else {
            // check for dead clients.
            if ((cur_time - my_metadata->clients[i].last_heartbeat_time)
                    > (HEARTBEAT_TIMEOUT))
            {
                SLOG("No heartbeat detected for METADATA_TIMEOUT * 3, assuming waiting client %d dead",
                        my_metadata->clients[i].client_id
                        );
                Remove_client_from_list(i);
            }
        }
    }

    if (my_metadata->client_count == MAX_SIM_CLIENTS) {
        LOG( "Max client count (%d) exceeded. "
             "Try to recompile with bigger MAX_SIM_CLIENTS value", MAX_SIM_CLIENTS);
        current_completion_handler.Set_args(ACQUIRE_RESULT_TOO_MANY_CLIENTS, nullptr);
        return false;
    }

    if (found_myself) {
        LOG("Shared memory already acquired");
        current_completion_handler.Set_args(ACQUIRE_RESULT_ALREADY_ACQUIRED, nullptr);
        return false;
    } else {
        // Put myself in clients list as waiting writer.
        auto my_index = my_metadata->client_count++;
        my_metadata->clients[my_index].client_id = my_client_id;
        my_metadata->clients[my_index].last_heartbeat_time = cur_time;
    }
    return true;
}

void
Shared_data::Main_loop()
{
    bool exit_main_loop = false;

    while(!exit_main_loop) {

        int my_idle_timeout_counter = 0;
        bool exit_acquire_loop = false;

        std::unique_lock<std::mutex> main_loop_locker(thread_mutex);
        tread_event.wait(main_loop_locker, [&](){return event_acquire || event_stop;});

        main_loop_locker.unlock();

        Master_lock();
        exit_main_loop = event_stop;
        if (event_acquire) {
            if (!Insert_client_in_list()) {
                current_request->Process(true);
                // release on behalf of user.
                exit_acquire_loop = true;
                event_acquire = false;  // acquire processing finished
                event_release = false;
                event_release_valid = false;
                acquire_in_progress = false;
            }
        }
        Master_release();


        while (!exit_acquire_loop)
        {
            Shared_semaphore::Lock_result gate_result;
            if (exit_main_loop) {
                gate_result = Shared_semaphore::WAIT_RESULT_TIMEOUT;
            } else {
                gate_result = Data_gate_wait();
            }

            auto master_lock_result = Master_lock();

            exit_main_loop = event_stop;

            if (    event_acquire
                &&  !event_stop
                &&  master_lock_result == ACQUIRE_RESULT_OK_RECOVERED) {
                if (!Insert_client_in_list()) {
                    if (gate_result == Shared_semaphore::WAIT_RESULT_OK) {
                        LOG("%d - Releasing master lock after failed reinit", my_client_id);
                        // Release data_gate only if we got the lock but
                        // failed to insert myself in list.
                        Data_gate_open();
                    }
                    current_request->Process(true);
                    // release on behalf of user.
                    exit_acquire_loop = true;
                    event_acquire = false;   // mark acquire processed.
                    event_release = false;
                    event_release_valid = false;
                    acquire_in_progress = false;
                    Master_release();
                    break; // out of acquire loop.
                }
            }

            Dump_client_list(my_client_id);

            // Check if current active client is alive.
            // if master locker timeouted then signal Data_gate_open
            // assuming that dead client is not able to signal it.
            auto cur_time = std::chrono::steady_clock::now();
            auto force_open_data_gate_on_timeout = false;
            for (uint32_t i = 0; i < my_metadata->client_count; i++) {
                // check for dead clients.
                if ((cur_time - my_metadata->clients[i].last_heartbeat_time)
                        > HEARTBEAT_TIMEOUT)
                {
                    SLOG("No heartbeat detected for HEARTBEAT_TIMEOUT, "
                            "assuming client %d dead",
                            my_metadata->clients[i].client_id);

                    Remove_client_from_list(i);

                    if (i == 0) {
                        if (gate_result == Shared_semaphore::WAIT_RESULT_TIMEOUT) {
                            LOG("%d - Releasing master lock on behalf of dead active client", my_client_id);
                            force_open_data_gate_on_timeout = true;
                        } else {
                            SLOG("%d - Taking over the lock from dead active client", my_client_id);
                        }
                    }
                }
            }

            int my_index = -1;

            // Check if I'm still considered alive (I exist in the clients list)
            for (uint32_t i = 0; i < my_metadata->client_count; i++) {
                if (my_metadata->clients[i].client_id == my_client_id) {
                    if (event_stop) {
                        current_completion_handler.Set_args(ACQUIRE_RESULT_DESTROYED, nullptr);
                        Remove_client_from_list(i);
                    } else if (event_release || event_release_valid) {
                        current_completion_handler.Set_args(ACQUIRE_RESULT_RELEASED, nullptr);
                        Remove_client_from_list(i);
                    } else if (event_acquire) {
                        if (current_request->Get_status() == Request::Status::CANCELLATION_PENDING) {
                            if (current_request->Timed_out()) {
                                SLOG("%d - request timeouted", my_client_id);
                                current_completion_handler.Set_args(ACQUIRE_RESULT_TIMEOUT, nullptr);
                            } else {
                                SLOG("%d - request canceled", my_client_id);
                                current_completion_handler.Set_args(ACQUIRE_RESULT_CANCELED, nullptr);
                            }
                            Remove_client_from_list(i);
                        } else {
                            // still good to wait for data gate lock.
                            my_metadata->clients[i].last_heartbeat_time = cur_time;
                            my_index = i;
                            SLOG("%d - still waiting", my_client_id);
                        }
                    }
                    break;
                }
            }

            if (my_index == -1) {
                // Our signature not found any more in waiter list.
                // Our Acquire failed...
                // One scenario is user called Release before we even got a chance to acquire.
                SLOG("%d - Release while waiting", my_client_id);
                if (event_acquire) {
                    current_request->Process(true);
                    event_acquire = false;
                }
                event_release = false;
                event_release_valid = false;
                acquire_in_progress = false;
                exit_acquire_loop = true;
            }

            if (gate_result == Shared_semaphore::WAIT_RESULT_OK) {
                // master lock acquired!
                if (my_index == 0) {

                    // Call user to notify that memory protecting lock is acquired.
                    switch (my_metadata->data_state) {
                    case DATA_STATE_CREATED:
                        current_completion_handler.Set_args(ACQUIRE_RESULT_OK_CREATED, my_data);
                        break;
                    case DATA_STATE_RECOVERED:
                        current_completion_handler.Set_args(ACQUIRE_RESULT_OK_RECOVERED, my_data);
                        break;
                    case DATA_STATE_OK:
                        current_completion_handler.Set_args(ACQUIRE_RESULT_OK, my_data);
                        break;
                    }
                    current_request->Process(true);
                    event_acquire = false;      // I'm ready to receive next acquire event.

                    // Release the metadata.
                    Master_release();

                    // Start heartbeating so that other processes can detect
                    // if this process has died and unlock the lock.
                    SLOG("%d - Acquired! Starting heartbeat.", my_client_id);

                    while (!exit_acquire_loop) {
                        std::unique_lock<std::mutex> hb_lock(thread_mutex);

                        if (!event_stop && !event_release && !event_release_valid) {
                            // nothing happened, sleep for 1 second or some event.
                            tread_event.wait_for(hb_lock, METADATA_TIMEOUT);
                        }
                        hb_lock.unlock();

                        Master_lock();
                        if (    my_metadata->clients[0].client_id == my_client_id
                            &&  my_metadata->client_count)
                        {// I'm still in the list.
                            if (event_stop || event_release || event_release_valid)
                            {// some event occurred
                                if (event_release_valid) {
                                    my_metadata->data_state = DATA_STATE_OK;
                                }
                                Remove_client_from_list(0);
                                Data_gate_open();

                                event_release = false;
                                event_release_valid = false;

                                exit_acquire_loop = true; // break out of acquiring while.
                            } else {
                                // timeout. update my heartbeat.
                                SLOG("%d - still keeping data gate", my_client_id);
                                my_metadata->clients[0].last_heartbeat_time = std::chrono::steady_clock::now();
                            }
                        } else {
                            SLOG("%d - Somebody thought I'm dead while active", my_client_id);
                            event_release = false;
                            event_release_valid = false;
                            acquire_in_progress = false;
                            exit_acquire_loop = true; // break out of acquiring while.
                        }
                        Master_release();
                    } // while !exit_acquire_loop
                    SLOG("%d - Acquire leaving", my_client_id);
                } else {

                    // got lock but I'm not the first in row.
                    SLOG("Acquired but my_index=%d", my_index);

                    Data_gate_open();

                    Master_release();

                    if (my_index > 0) {
                        // Sleep a little to yield other waiter if there is any.
                        // If the other waiter is dead it will eventually
                        // get removed from the client list by dead client detection code above.
                        SLOG("Acquire yield for %d ms", my_index);
                        std::this_thread::sleep_for(std::chrono::milliseconds(my_index));
                    }
                }
            } else if (gate_result == Shared_semaphore::WAIT_RESULT_TIMEOUT) {
                if (    my_index == 0
                    &&  my_idle_timeout_counter++ > METADATA_TIMEOUT_COUNT_MAX)
                {
                    LOG("%d - Releasing master lock on behalf of myself", my_client_id);
                    force_open_data_gate_on_timeout = true;
                    my_idle_timeout_counter = 0;
                }
                if (force_open_data_gate_on_timeout) {
                    Data_gate_open();
                }
                Master_release();
            }
        } //while !exit_acquire_loop
    }// while !exit_main_loop
}

Operation_waiter
Shared_data::Acquire(
        Acquire_handler completion_handler,
        Request_completion_context::Ptr completion_context)
{
    SLOG("%d - Acquire called", my_client_id);
    auto request = Request::Create();

    request->Set_completion_handler(
            completion_context,
            completion_handler);

    auto proc_handler = Make_callback([](Request::Ptr r){r->Complete();}, request);

    request->Set_processing_handler(proc_handler);

    std::unique_lock<std::mutex> lk(thread_mutex);

    if (!acquire_in_progress) {
        // notify thread of new acquire.
        event_acquire = true;
        acquire_in_progress = true;
        current_completion_handler = completion_handler;
        current_request = request;
        tread_event.notify_all();
    } else {
        // complete the request directly from here.
        completion_handler.Set_args(ACQUIRE_RESULT_ALREADY_ACQUIRED, nullptr);
        request->Process(true);
    }
    return request;
}

void
Shared_data::Release(bool data_valid)
{
    SLOG("%d - Release called", my_client_id);
    std::unique_lock<std::mutex> lk(thread_mutex);
    if (acquire_in_progress) {
        // notify thread of release.
        if (data_valid) {
            event_release_valid = true;
        } else {
            event_release = true;
        }
        acquire_in_progress = false;
        tread_event.notify_all();
    } else {
        SLOG("%d - Release called in invalid state", my_client_id);
    }
}

Shared_semaphore::Lock_result
Shared_data::Data_gate_wait()
{
    Shared_semaphore::Lock_result ret;
    ret = data_gate->Wait(METADATA_TIMEOUT);
    if (ret == Shared_semaphore::WAIT_RESULT_ERROR) {
        VSM_EXCEPTION(Exception, "Failed to acquire Data_gate lock!");
    }

    if (ret == Shared_semaphore::WAIT_RESULT_OK) {
        SLOG("%d - v====================================================v Data locked", my_client_id);
    } else {
        SLOG("%d - = Data lock timeout =", my_client_id);
    }
    return ret;
}

void
Shared_data::Data_gate_open()
{
    SLOG("%d - ^====================================================^ Data released", my_client_id);
    data_gate->Signal();
}

void
Shared_data::Master_release()
{
    thread_mutex.unlock();
    ASSERT(my_metadata->current_client_id == my_client_id);
    my_metadata->current_client_id = 0;
    SLOG("%d - ^----------------------^ Master RELEASED", my_client_id);
    master_locker->Signal();
}

} /* namespace vsm */
