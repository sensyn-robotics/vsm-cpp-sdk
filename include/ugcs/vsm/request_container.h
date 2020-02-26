// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file request_container.h
 *
 * Request container declaration.
 */

#ifndef _UGCS_VSM_REQUEST_CONTAINER_H_
#define _UGCS_VSM_REQUEST_CONTAINER_H_

#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/utils.h>

#include <memory>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <list>

namespace ugcs {
namespace vsm {

/** Generic container for queued requests. It provides interface for submitting
 * requests into the container and executing associated handlers for queued requests.
 */
class Request_container: public std::enable_shared_from_this<Request_container> {
    DEFINE_COMMON_CLASS(Request_container, Request_container)

public:
    /** Generic request for implementing inter-threads communications and asynchronous
     * operations.
     */
    class Request: public std::enable_shared_from_this<Request> {
        DEFINE_COMMON_CLASS(Request, Request)

    public:
        /** Request processing status which is returned by the handler or set
         * internally.
         */
        enum class Status {
            /** Request is currently pending for processing. Should never be returned
             * from handlers.
             */
            PENDING,
            /** Request is not yet taken from processor input queue and Cancel()
             * method was called.
             */
            CANCELLATION_PENDING,
            /** Request was taken from processor input queue in CANCELLATION_PENDING
             * state.
             */
            CANCELING,
            /** Request processing handler was called and request is being processed.
             * Should never be returned from the handler.
             */
            PROCESSING,
            /** Processing was aborted by calling Abort() method and request is
             * submitted to the completion context for a completion handler
             * release to be made. This is necessary to safely release the
             * completion handler which might induce side effects potentially
             * blocking the Abort() method caller and also to make sure that
             * possible side effects are happening in a user provided context
             * and not in the context of Abort() method caller.
             */
            ABORT_PENDING,
            /** Processing was aborted by calling Abort() method and full
             * cleanup is made. */
            ABORTED,

            /** Result codes passed to Complete() method should start from this
             * value.
             */
            RESULT_CODES,

            /** Request successfully processed. */
            OK = RESULT_CODES,
            /** Request was canceled. */
            CANCELED
        };

        /** Callback denoting a handler of the request. */
        typedef Callback_base<void>::Ptr<> Handler;
        /** Smart lock object for request external locking. */
        typedef std::unique_lock<std::mutex> Locker;

        virtual
        ~Request();

        /** Set processing handler for the request. It is called when request is about to
         * be processed in the target processor context.
         *
         * @param handler Processing handler to invoke.
         * @throws Invalid_op_exception if request not in pending state.
         */
        void
        Set_processing_handler(const Handler &handler);

        /** @see Set_processing_handler */
        void
        Set_processing_handler(Handler &&handler);

        /** Set completion handler for the request. It is called when request is completed
         * in the specified completion context.
         *
         * @param context Context where completion handler should be invoked.
         * @param handler Completion handler to call when request is processed.
         * @throws Invalid_op_exception if request not in pending state.
         */
        void
        Set_completion_handler(const Request_container::Ptr &context,
                               const Handler &handler);

        /** @see Set_completion_handler */
        void
        Set_completion_handler(const Request_container::Ptr &context,
                               Handler &&handler);

        /** Set request cancellation handler. It is fired when Abort() method is
         * called between request processing started and Complete() is called (i.e.
         * while request is in PROCESSING state). The second case is calling request
         * Cancel() method when request is either in input queue or being processed.
         * Correct handling in the handler is taking lock on the request and
         * checking its state (either PROCESSING or ABORTED). In PROCESSING
         * state it is legal to call Complete() method (either instantly or by
         * deferred invocation), or do nothing if cancellation is not supported
         * or cannot be done in current state. In ABORTED state all platform
         * resources should be released, no further submits for the request will be
         * done.
         *
         * @param handler Cancellation handler.
         * @throws Invalid_op_exception if request is not in PENDING state.
         */
        void
        Set_cancellation_handler(const Handler &handler);

        /** @see Set_cancellation_handler */
        void
        Set_cancellation_handler(Handler &&handler);

        /** Set request done handler. The handler is invoked when request is advanced
         * to done state - no more actions are pending for it and it is
         * destroyed. The handler is always called - in both normal completion
         * or abortion scenario. If request is already done when this function is
         * called, the handler is invoked immediately. Calling context is either
         * completion, abortion or this method calling context.
         */
        void
        Set_done_handler(Handler &handler);

        /** @see Set_done_handler. */
        void
        Set_done_handler(Handler &&handler);

        /** Get completion context associated with the request. It might be null if
         * request is already fully processed or not yet fully prepared.
         */
        Request_container::Ptr
        Get_completion_context(Locker locker = Locker()) const;

        /** Process the request. Either processing or completion context is called
         * depending on request state.
         *
         * @param process_request true if request processing is intended or
         *      false if completion notification processing is intended.
         * @throws Invalid_op_exception if the method is called more than once for
         *      processing and notification handling, or calling sequence is invalid.
         * @throws Nullptr_exception if corresponding handler is not set.
         */
        void
        Process(bool process_request);

        /** Complete the request processing. Request with the specified status is
         * submitted to the associated completion context.
         *
         * @param status Completion status.
         * @param locker Locker object which can have previously acquired
         *      lock for request internal state. It is released before the function
         *      exits.
         * @throws Invalid_param_exception if invalid status value specified.
         * @throws Invalid_op_exception if request is not in allowed state (e.g.
         *      processing not yet started).
         */
        void
        Complete(Status status = Status::OK, Locker locker = Locker());

        /** Cancel request processing. It does nothing if request already processed.
         * This action behavior is defined by specific operation and processor.
         * If unsure, it is always recommended to use Abort() instead of Cancel().
         *
         * @param locker Locker object which can have previously acquired
         *      lock for request internal state. It is released before the function
         *      exits.
         */
        void
        Cancel(Locker locker = Locker());

        /** Call this method when request is not going to be processed by Process()
         * method but is just removed from request queue. It also can be called
         * when request is queued in some container so it later will be ignored
         * when the container will try to process it. This method does nothing
         * if request was already aborted or fully processed.
         *
         * @param locker Locker object which can have previously acquired
         *      lock for request internal state. It is released before the function
         *      exits.
         */
        void
        Abort(Locker locker = Locker());

        /** Acquire lock for request internal state. It is safe to assume that
         * request state will not be changed while the lock is acquired (e.g.
         * aborted). Use std::move to pass the returned object to Complete() or
         * Abort() method if you atomic request completion or aborting.
         *
         * @param acquire Should the lock be acqired or not.
         *
         * @return Lock object.
         */
        Locker
        Lock(bool acquire = true) const;

        /** Get request current status. */
        Status
        Get_status() const
        {
            return status;
        }

        /** Check if request is completed. */
        bool
        Is_completed() const
        {
            return status >= Status::RESULT_CODES;
        }

        /** Return true only if the request processing (i.e. Process(true)) is
         * needed for this request, otherwise false.
         */
        bool
        Is_request_processing_needed() const
        {
            /* Abort pending is only for completion contexts. */
            return !Is_completed() && Get_status() != Status::ABORT_PENDING;
        }

        /** Check if request is aborted. */
        bool
        Is_aborted() const
        {
            return status == Status::ABORTED || status == Status::ABORT_PENDING;
        }

        /** Check if request is still processing. */
        bool
        Is_processing() const
        {
            return status == Status::PROCESSING;
        }

        /** Check if the request completion notification is delivered. */
        bool
        Is_completion_delivered() const
        {
            return completion_delivered;
        }

        /** Check if the request completion handler already invoked (but might not
         * be returned yet.
         */
        bool
        Is_completion_delivering_started() const
        {
            return completion_processed && !completion_handler;
        }

        /** Check if request is fully processed, i.e. all handlers were invoked and
         * no more actions pending.
         */
        bool
        Is_done() const
        {
            return Is_completion_delivered() || Is_aborted();
        }

        /** Wait for request is fully processed, i.e. all handlers were invoked and
         * no more actions pending.
         *
         * @param process_ctx Process all requests in the associated completion
         *      context. This may be useful if the context is normally
         *      processed in the calling thread.
         * @param timeout Timeout value for waiting. Zero value indicates
         *      indefinite waiting.
         * @return "true" if request was processed, "false" if the function
         *      returned by timeout expiration.
         */
        bool
        Wait_done(bool process_ctx = true,
                  std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

        /** Access timed_out member. This should be set/read under request lock */
        bool&
        Timed_out()
        {
            return timed_out;
        }

    protected:
        /** Called to destroy request. Primarily should be used by derived classes
         * to destroy circular references if such exist.
         */
        virtual void
        Destroy();

        /**
         * Checks the availability of the completion handler. Request should
         * be locked by the caller.
         * XXX This is a workaround for the Io_request bad design related to
         * references to the result and buffer arguments.
         */
        bool
        Is_completion_handler_present();

    private:
        /** Request processing handler. Called when request is about to be processed. */
        Handler processing_handler;
        /** Request completion handler. Called when request is completed. */
        Handler completion_handler;
        /** Cancellation handler is called when request is aborted in processing
         * state.
         */
        Handler cancellation_handler;
        /** Done handler is invoked when request is advanced to done state - no
         * more actions are pending for it and it is destroyed.
         */
        Handler done_handler;
        /** Context where completion handler should be called. */
        Request_container::Ptr completion_context;
        /** Request processing status. S_PENDING while it is waiting for processing.
         * Processing status after that.
         */
        /** request has timed out. This must be set/read under request lock */
        bool timed_out = false;
        std::atomic<Status> status = { Status::PENDING };
        /** Was the Complete() method invoked. */
        std::atomic_bool completion_processed = { false },
        /** Was the completion handler invoked and returned. */
                         completion_delivered = { false };
        /** Mutex for protecting state modifications. */
        mutable std::mutex mutex;
        /** Condition variable for request state changes. */
        std::condition_variable cond_var;
    };

    /** Represents request synchronization entity which is used for request queues
     * protected access, submission notifications and waiting.
     */
    class Request_waiter: public std::enable_shared_from_this<Request_waiter> {
        DEFINE_COMMON_CLASS(Request_waiter, Request_waiter)

    public:
        /** Predicate for wait operations.
         * @return "true" if wait condition met, "false" otherwise.
         */
        typedef Callback_base<bool>::Ptr<> Predicate;

        class Locker;

        /** Notify all listeners about request submission. */
        virtual void
        Notify();

        /** Acquire lock for request getting. */
        Locker
        Lock();

        /** Acquire lock for request submission.
         *
         * @return Locker object which own the acquired lock (RAII helper). Once the
         *      locker is destroyed the waiter is unlocked and notified.
         */
        Locker
        Lock_notify();

        /** Wait for request submission. It blocks until request submitted or the
         * specified timeout elapses. If there are some requests submitted they
         * are processed. After processing the method exits.
         *
         * @param containers List of containers to check and wait for.
         * @param timeout Timeout in milliseconds. Zero value indicates indefinite
         *      waiting.
         * @param requests_limit Limit of requests to process at once. Zero means no
         *      limit.
         * @param predicate Predicate to check during waiting. It overrides
         *      default predicate which checks number of processed requests.
         * @return Number of requests processed.
         */
        int
        Wait_and_process(const std::initializer_list<Request_container::Ptr> &containers,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
                         int requests_limit = 0, Predicate predicate = Predicate());

        /** Wait for request submission. It blocks until request submitted or the
         * specified timeout elapses. If there are some requests submitted they
         * are processed. After processing the method exits.
         *
         * @param containers List of containers to check and wait for.
         * @param timeout Timeout in milliseconds. Zero value indicates indefinite
         *      waiting.
         * @param requests_limit Limit of requests to process at once. Zero means no
         *      limit.
         * @param predicate Predicate to check during waiting. It overrides
         *      default predicate which checks number of processed requests.
         * @return Number of requests processed.
         */
        int
        Wait_and_process(const std::list<Request_container::Ptr> &containers,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
                         int requests_limit = 0, Predicate predicate = Predicate());

        virtual
        ~Request_waiter() = default;

    private:
        friend class Locker;

        /** Mutex for submitting and waiting/getting requests. */
        std::mutex mutex;
        /** Condition variable for waiting and notifying about requests. */
        std::condition_variable cond_var;

        /** Implementation for public methods Wait_and_process.
         * @see Wait_and_process
         */
        template <class Container_list>
        int
        Wait_and_process_impl(const Container_list &containers,
                              std::chrono::milliseconds timeout,
                              int requests_limit, Predicate ext_predicate);
    };

    /** Container type. */
    enum class Type {
        /** None type used in base class. */
        NONE                            = 0x0,
        /** Request processor. */
        PROCESSOR                       = 0x1,
        /** Request completion context. */
        COMPLETION_CONTEXT              = 0x2,
        /** Container capable of handling both requests and completions. */
        ANY                             = PROCESSOR | COMPLETION_CONTEXT,
        /** Temporal context used for a separate request synchronization. */
        TEMPORAL                        = 0x4,
        /** Temporal completion context. */
        TEMP_COMPLETION_CONTEXT         = COMPLETION_CONTEXT | TEMPORAL
    };

    /** Create container with default associated waiter. */
    Request_container(
            const std::string& name,
            Request_waiter::Ptr waiter = Request_waiter::Create());

    virtual
    ~Request_container();

    /** Get this container type. */
    virtual Type
    Get_type() const
    {
        return Type::NONE;
    }

    /** Check if the container type conforms the specified characteristic mask. */
    bool
    Check_type(Type mask)
    {
        return static_cast<int>(Get_type()) & static_cast<int>(mask);
    }

    /** Submit request to this container for further processing or notification
     * handlers invocation.
     *
     * @param request Request to submit.
     */
    void
    Submit_request(Request::Ptr request);

    /** The same as Submit_request, but with previously acquired lock of the
     * associated waiter. Lock should be with notify.
     */
    void
    Submit_request_locked(
                Request::Ptr request,
                Request_waiter::Locker locker);

    /** Process all currently queued requests.
     *
     * @param requests_limit Limit of requests to process at once. Zero means no limit.
     * @return Number of requests processed.
     */
    int
    Process_requests(int requests_limit = 0);

    /** Process all currently queued requests. This version does not lock the
     * waiter but uses provided mutex instead assuming it is locked when called.
     * Intended for use from Request_waiter::Wait_and_process.
     *
     * @param lock External lock to use for protecting queue access.
     * @param requests_limit Limit of requests to process at once. Zero means no limit.
     * @return Number of requests processed.
     */
    int
    Process_requests(std::unique_lock<std::mutex> &lock, int requests_limit = 0);

    /** Set request waiter associated with this container.
     *
     * @param waiter Waiter object.
     */
    void
    Set_waiter(Request_waiter::Ptr waiter);

    /** Get request waiter associated with this container. */
    Request_waiter::Ptr
    Get_waiter() const
    {
        return waiter;
    }

    /** Get the name of the container. */
    const std::string&
    Get_name()
    {
        return name;
    }

    /** Enable the container. The container is ready to accept requests after that.
     * Derived class can start dedicated threads there.
     * @throws Invalid_op_exception if the container is already enabled.
     */
    void
    Enable();

    /** Disable the container. All requests which are submitted after this
     * method is called will be aborted. It is up to container implementation
     * to decide whether to process or abort requests which are already submitted
     * or being processed. If a container has dedicated threads, it should
     * terminate them (should be done synchronously in On_disable() method).
     * Method must be called from the same thread as @ref Enable method.
     */
    void
    Disable();

    /** Check if the container is currently enabled. */
    bool
    Is_enabled() const;

protected:
    /** Waiter associated with this container. It is used to synchronize access
     * to the request queue in derived classes.
     */
    Request_waiter::Ptr waiter;
    /** Queue of pending requests, i.e. waiting for completion notification
     * processing.
     */
    std::list<Request::Ptr> request_queue;

    /** Request processing loop implementation. It does not return while the
     * container is enabled.
     */
    void
    Processing_loop();

    /** Process request.
     * @param request Request to process.
     */
    virtual void
    Process_request(Request::Ptr request) = 0;

    /** Called when the container is enabled. Derived classes should override
     * this method to perform container-specific enabling, e.g. launch dedicated
     * threads.
     */
    virtual void
    On_enable();

    /** Called when the container is terminated. Derived classes should override
     * this method to perform container-specific disabling, i.e. abort all
     * pending operations related to this context and synchronously
     * terminate dedicated threads.
     * Set_disabled() method should be called by a derived class from inside
     * this method when appropriate.
     */
    virtual void
    On_disable();

    /** Disable the container and notify its waiter. */
    void
    Set_disabled();

    /** Called when default processing loop want to wait for work and process
     * it. Can be overridden by derived classes if they have non-trivial wait
     * and process implementation.
     */
    virtual void
    On_wait_and_process();

private:
    /** Implementation of the request submit method with a locked locker. */
    void
    Submit_request_impl(
            Request::Ptr request,
            Request_waiter::Locker locker);

    /** Abort all requests which are currently queued in request queue. */
    void
    Abort_requests();

    /** Indicates the container is currently enabled. */
    std::atomic_bool is_enabled = { false };

    /** Indicate that disable method is already called. */
    std::atomic_bool disable_ongoing = { false };

    /** Indicates the ongoing process of aborting all currently queued requests.
     * It is possible to submit requests only in abort pending
     * state in this phase.
     */
    std::atomic_bool abort_ongoing = { false };

    /** Human readable name of the container to ease the debugging. */
    const std::string name;

    /** Queue of the requests being abort during context disabling. */
    std::list<Request::Ptr> aborted_request_queue;
};

/** Request waiter type for convenient usage. */
typedef Request_container::Request_waiter Request_waiter;

/** Helper class for RAII-based locking and notifying Request_waiter objects.
 * Obtained via Request_waiter::Lock and Request_waiter::Lock_notify methods.
 */
class Request_container::Request_waiter::Locker {
public:
    /** Construct locker object.
     *
     * @param waiter Associated waiter object.
     * @param want_notify Automatically call waiter Request_waiter::Notify()
     *      method when the lock is released.
     */
    Locker(Request_waiter::Ptr waiter, bool want_notify = false);

    ~Locker();

    /** Disable copying. */
    Locker(const Locker &) = delete;

    /** Enable moving. */
    Locker(Locker &&);

    /** Lock the waiter explicitly.
     * @throws Invalid_op_exception if already locked.
     */
    void
    Lock();

    /** Unlock the waiter explicitly.
     * @throws Invalid_op_exception if already unlocked.
     */
    void
    Unlock();

    /** Is locker wants a notify. */
    bool
    Want_notify() const;

    /** Check if locker is bound to this waiter. */
    bool
    Is_same_waiter(const Request_waiter::Ptr&) const;

private:
    /** Associated waiter. */
    Request_waiter::Ptr waiter;
    /** Indicates whether notification needed after unlocking. */
    bool want_notify;
    /** Indicates that waiter is locked. */
    bool is_locked;
};

/** Request type for convenient usage. */
typedef Request_container::Request Request;

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_REQUEST_CONTAINER_H_ */
