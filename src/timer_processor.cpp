// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Timer processor.
 */

#include <ugcs/vsm/timer_processor.h>

using namespace ugcs::vsm;

/* Timer_processor::Timer class implementation. */

Timer_processor::Timer::Timer(const Timer_processor::Ptr &processor,
                              std::chrono::milliseconds interval):
    processor(processor),
    interval(interval),
    fire_time(std::chrono::steady_clock::now() + interval)
{
}

void
Timer_processor::Timer::Cancel()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (!is_running) {
        return;
    }
    Timer_processor::Ptr proc = processor;
    lock.unlock();
    if (proc) {
        proc->Cancel_timer(Shared_from_this());
    }
}

bool
Timer_processor::Timer::Is_running() const
{
    std::unique_lock<std::mutex> lock(mutex);
    return is_running;
}

void
Timer_processor::Timer::Fire()
{
    std::unique_lock<std::mutex> lock(mutex);
    for (auto timer: attached_timers) {
        timer->Fire();
    }
    attached_timers.clear();
    if (!is_running) {
        /* Destroyed timer can be in tree if it is canceled during timer request
         * processing.
         */
        return;
    }
    request->Complete();
}

void
Timer_processor::Timer::Destroy(bool cancel)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (!is_running) {
        return;
    }
    is_running = false;
    if (request) {
        if (cancel) {
            auto req_lock = request->Lock();
            if (!request->Is_completion_delivered()) {
                request->Abort(std::move(req_lock));
            }
        } else {
            request->Abort();
        }
        request = nullptr;
    }
    ASSERT(attached_timers.empty());
    processor = nullptr;
}

void
Timer_processor::Timer::Attach(Timer::Ptr &timer)
{
    std::unique_lock<std::mutex> lock(mutex);
    attached_timers.push_back(timer);
}

void
Timer_processor::Timer::Attach(std::list<Ptr> &&timers)
{
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT(attached_timers.empty());
    attached_timers = std::move(timers);
}

void
Timer_processor::Timer::Detach(Timer::Ptr &timer)
{
    std::unique_lock<std::mutex> lock(mutex);
    attached_timers.remove(timer);
}

Timer_processor::Timer::Ptr
Timer_processor::Timer::Get_attached()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (attached_timers.empty()) {
        return Ptr();
    }
    Ptr repl_timer = attached_timers.front();
    attached_timers.pop_front();
    std::list<Ptr> timers(std::move(attached_timers));
    /* Move lock to the target timer. */
    lock = std::unique_lock<std::mutex>(repl_timer->mutex);
    ASSERT(repl_timer->attached_timers.empty());
    repl_timer->attached_timers = std::move(timers);
    return repl_timer;
}

void
Timer_processor::Timer::Set_request(Request::Ptr &request)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (this->request) {
        this->request->Abort();
    }
    this->request = request;
}

/* Timer_processor class implementation. */

Singleton<Timer_processor> Timer_processor::singleton;

Timer_processor::Timer_processor():
    Request_processor("Timer processor")
{
}

Timer_processor::Tick_type
Timer_processor::Get_ticks(const std::chrono::steady_clock::time_point &time)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (time.time_since_epoch()).count();
}

void
Timer_processor::Timer_process_handler(Timer::Ptr timer)
{
    if (!timer->Is_running()) {
        /* Already canceled. */
        return;
    }
    /* Check if it still needs to be placed in tree. */
    auto now = std::chrono::steady_clock::now();
    if (timer->Get_fire_time() <= now) {
        timer->Fire();
        return;
    }
    Insert_timer(timer);
}

void
Timer_processor::Insert_timer(Timer::Ptr &timer)
{
    std::unique_lock<std::mutex> lock(tree_lock);
    auto result = tree.insert(std::pair<Tick_type, Timer::Ptr>
        (Get_ticks(timer->Get_fire_time()), timer));
    if (!result.second) {
        /* Same slot already occupied, attach to existing timer. */
        result.first->second->Attach(timer);
    }
}

void
Timer_processor::Timer_handler(Timer::Ptr timer, Handler handler,
                               Request_container::Ptr container)
{
    if (handler() && timer->Is_running()) {
        timer->fire_time += timer->interval;
        /* Do not allow to accumulate firings. */
        auto now = std::chrono::steady_clock::now();
        if (timer->fire_time < now) {
            timer->fire_time = now;
        }
        Create_request(timer, handler, container);
    } else {
        timer->Destroy();
    }
}

void
Timer_processor::Create_request(Timer::Ptr &timer, const Handler &handler,
                             Request_container::Ptr &container)
{
    Request::Ptr request = Request::Create();
    request->Set_processing_handler(
        Make_callback(&Timer_processor::Timer_process_handler, this, timer));
    request->Set_completion_handler(container,
        Make_callback(&Timer_processor::Timer_handler, this, timer, handler,
                      container));
    timer->Set_request(request);
    Submit_request(request);
}

Timer_processor::Timer::Ptr
Timer_processor::Create_timer(std::chrono::milliseconds interval,
                              const Handler &handler,
                              Request_container::Ptr container)
{
    if (!handler || !container) {
        VSM_EXCEPTION(Invalid_param_exception, "Both timer handler and container "
                "should be set.");
    }
    Timer::Ptr timer = Timer::Create(Shared_from_this(), interval);
    Create_request(timer, handler, container);
    return timer;
}

void
Timer_processor::Cancel_timer(Timer::Ptr timer)
{
    std::unique_lock<std::mutex> lock(tree_lock);
    Tick_type ticks = Get_ticks(timer->Get_fire_time());
    do {
        auto it = tree.find(ticks);
        if (it == tree.end()) {
            break;
        }
        if (it->second != timer) {
            it->second->Detach(timer);
        } else {
            /* Replace node by attached timers. */
            Timer::Ptr repl_timer = timer->Get_attached();
            tree.erase(it);
            if (repl_timer) {
                tree.insert(std::pair<Tick_type, Timer::Ptr>(ticks, repl_timer));
            }
        }
    } while (false);
    timer->Destroy(true);
}

void
Timer_processor::On_enable()
{
    Request_processor::On_enable();
    thread = std::thread(&Timer_processor::Processing_loop, Shared_from_this());
}

void
Timer_processor::On_disable()
{
    Set_disabled();
    /* Wait for dedicated thread terminates. */
    thread.join();
    std::unique_lock<std::mutex> lock(tree_lock);
    for(auto& iter: tree) {
        if (!iter.second->Is_running()) {
            continue;
        }
        auto req = iter.second->request;
        std::string ctx_name = "absent";
        if (req) {
            auto locker = req->Lock();
            if (req->Get_completion_context()) {
                ctx_name = req->Get_completion_context()->Get_name();
            }
        }
        LOG_ERR("Timer interval [%" PRIu64 " ms] in context [%s] is still running.",
                static_cast<uint64_t>(iter.second->interval.count()),
                ctx_name.c_str());
        /* This is not normal. Timer users should cancel their timers before
         * disabling the timer processor. Try to recover in release anyway. */
        ASSERT(false);
    }
    /* Cancel all running timers. */
    while (!tree.empty()) {
        auto timer = tree.begin()->second;
        lock.unlock();
        Cancel_timer(timer);
        lock.lock();
    }
}

void
Timer_processor::On_wait_and_process()
{
    std::unique_lock<std::mutex> lock(tree_lock);
    while (true) {
        auto now = std::chrono::steady_clock::now();
        /* Get nearest timer to fire. */
        if (tree.empty()) {
            /* Wait indefinitely. */
            lock.unlock();
            this->waiter->Wait_and_process({Shared_from_this()});
            break;
        }
        auto it = tree.begin();
        Timer::Ptr timer = it->second;
        std::chrono::milliseconds delay =
            std::chrono::duration_cast<std::chrono::milliseconds>
            (timer->Get_fire_time().time_since_epoch() - now.time_since_epoch());
        if (delay.count() > 0) {
            /* Wait until this timer. */
            lock.unlock();
            this->waiter->Wait_and_process({Shared_from_this()},
                                           delay);
            /* If waken up by timeout, timers will be fired during next iteration. */
            break;
        }
        /* Some expired timers exist, fire them. */
        tree.erase(it);
        timer->Fire();
    }
}
