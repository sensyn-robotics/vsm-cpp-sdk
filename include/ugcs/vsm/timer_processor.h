// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file timer_processor.h
 *
 * Timer processor. Provides asynchronous timers.
 */

#ifndef TIMER_PROCESSOR_H_
#define TIMER_PROCESSOR_H_

#include <ugcs/vsm/request_context.h>
#include <ugcs/vsm/singleton.h>

#include <thread>
#include <map>

namespace ugcs {
namespace vsm {

/** Timer processor manages all timers in the VSM. */
class Timer_processor: public Request_processor {
    DEFINE_COMMON_CLASS(Timer_processor, Request_container)
public:
    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    Timer_processor();

    /** Timer handler. It should return boolean value with the following
     * meanings:
     * false - stop the timer, i.e. do not perform further invocations;
     * true - re-schedule next invocation after the initial interval.
     */
    typedef Callback_proxy<bool> Handler;

    /** Represents timer instance. */
    class Timer: public std::enable_shared_from_this<Timer> {
        DEFINE_COMMON_CLASS(Timer, Timer)
    public:
        /** Construct timer instance associated with a processor. */
        Timer(const Timer_processor::Ptr &processor,
              std::chrono::milliseconds interval);

        /** Cancel running timer. Do nothing if timer is not running. */
        void
        Cancel();

        /** Check if the timer still is running - i.e. will produce handler
         * invocations.
         */
        bool
        Is_running() const;

        /** Get time of next timer firing. */
        std::chrono::steady_clock::time_point
        Get_fire_time() const
        {
            return fire_time;
        }

    private:
        friend class Timer_processor;

        /** Related processor. */
        Timer_processor::Ptr processor;
        /** Indicates that timer is currently running. */
        bool is_running = true;
        /** Timer interval. */
        std::chrono::milliseconds interval;
        /** Time when the timer should be fired next time. */
        std::chrono::steady_clock::time_point fire_time;
        /** Associated request. */
        Request::Ptr request;
        /** Mutex for state updates protection. */
        mutable std::mutex mutex;
        /** List of attached timers in the same slot. */
        std::list<Ptr> attached_timers;

        /** Set associated request. */
        void
        Set_request(Request::Ptr &request);

        /** Fire the timer (asynchronously, i.e. associated request is completed). */
        void
        Fire();

        /** Destroy when the timer is not longer needed (e.g. fired last time or
         * was canceled).
         * @param cancel Should be true if timer is canceled. Associated request
         *      is aborted if not yet too late.
         */
        void
        Destroy(bool cancel = false);

        /** Attach timer which occupies the same slot. */
        void
        Attach(Timer::Ptr &timer);

        /** Attach timers which were previously attached to another timer. */
        void
        Attach(std::list<Ptr> &&timers);

        /** Detach specified timer. */
        void
        Detach(Timer::Ptr &timer);

        /** Get attached timers (they are detached). */
        Ptr
        Get_attached();
    };

    /** Create and schedule the timer instance. First time it is fired after the
     * specified interval (since the creation moment). Is it periodic or
     * one-shot is defined by the provided handler - while it is returning "true"
     * the timer is re-scheduled with the same interval.
     *
     * @param interval Timer interval.
     * @param handler Handler to invoke, should be non-empty. See {@link Handler}.
     * @param container Container where the handler will be executed.
     * @return Timer object which can be used, for example, to cancel running
     *      timer.
     * @throw Invalid_param_exception if Handler or container is not set.
     */
    Timer::Ptr
    Create_timer(std::chrono::milliseconds interval, const Handler &handler,
                 Request_container::Ptr container);

    /** Cancel the specified timer in case it is running. */
    void
    Cancel_timer(Timer::Ptr timer);

private:
    /** Type used for indexing timers tree, effectively ticks counter type. */
    typedef decltype(std::chrono::milliseconds().count()) Tick_type;

    /** Dedicated processor thread. */
    std::thread thread;
    /** Timer tree. */
    std::map<Tick_type, Timer::Ptr> tree;
    /** Mutex for protecting tree access. */
    std::mutex tree_lock;
    /** Singleton object. */
    static Singleton<Timer_processor> singleton;

    /** Handle processor enabling. */
    virtual void
    On_enable() override;

    /** Handle disabling request. */
    virtual void
    On_disable() override;

    /** Implement own wait-and-process transaction. */
    virtual void
    On_wait_and_process() override;

    /** Called when timer request processing started. */
    void
    Timer_process_handler(Timer::Ptr timer);

    /** Called when timer request processing complete. */
    void
    Timer_handler(Timer::Ptr timer, Handler handler,
                  Request_container::Ptr container);

    /** Insert timer into the tree. */
    void
    Insert_timer(Timer::Ptr &timer);

    /** Get absolute ticks count from clock time. */
    static Tick_type
    Get_ticks(const std::chrono::steady_clock::time_point &time);

    /** Create and submit request for the specified timer. */
    void
    Create_request(Timer::Ptr &timer, const Handler &handler,
                   Request_container::Ptr &container);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* TIMER_PROCESSOR_H_ */
