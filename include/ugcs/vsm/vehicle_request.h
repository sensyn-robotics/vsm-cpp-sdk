// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vehicle_request.h
 * Generic asynchronous request for a user vehicle.
 */
#ifndef _VEHICLE_REQUEST_H_
#define _VEHICLE_REQUEST_H_

#include <ugcs/vsm/request_context.h>
#include <ugcs/vsm/reference_guard.h>
#include <ugcs/vsm/debug.h>

namespace ugcs {
namespace vsm {

/** Base class of a generic request for a vehicle. It provides reference
 * management methods for associated handles. */
class Vehicle_request: public std::enable_shared_from_this<Vehicle_request> {
    DEFINE_COMMON_CLASS(Vehicle_request, Vehicle_request)
public:

    /** Request completion result. */
    enum class Result {
        /** Completed successfully. */
        OK,
        /** Processing failed. */
        NOK
    };

    /** Handle of the request passed to the SDK user. Used to expose only
     * the necessary base interface for the user and hide other methods of the
     * main request class. Passed by value, as this is effectively only a
     * pointer wrapper.
     */
    class Handle {
    public:

        /** Construct initially invalid handle. */
        Handle();

        /** Construct handle instance with managed vehicle request. Called by SDK
         * only. */
        Handle(Vehicle_request::Ptr vehicle_request);

        /**
         * Handle validness check. Handle is considered valid if it points
         * to some vehicle request which is not yet completed.
         * @return true if handle is valid, otherwise false.
         */
        explicit operator bool() const;

        void
        Fail(const char *format = nullptr, ...) __FORMAT(printf, 2, 3);

        void
        Fail_v(const char *format, va_list fmt_args) __FORMAT(printf, 2, 0);

        void
        Fail(const std::string& reason);

        void
        Succeed();

    protected:

        /** Assignment operator implementation. */
        void
        Assign_result(Result result, const std::string& status_text = std::string());

        /** Managed vehicle request. */
        Reference_guard<Vehicle_request::Ptr> vehicle_request;
    };

    /** Completion handler type of the request. */
    typedef Callback_proxy<void, Result, std::string> Completion_handler;

    /** Construct base request instance. */
    Vehicle_request(Completion_handler completion_handler,
            Request_completion_context::Ptr completion_ctx);

    /** Make sure class is polymorphic. */
    virtual
    ~Vehicle_request();

    /**
     * Set completion result to be used when Complete method is called.
     * @param result Result for Complete method.
     */
    void
    Set_completion_result(Result result, const std::string&);

    /** Should be called when vehicle request is completed by the user. Result,
     * previously set by Set_completion_result is used. */
    void
    Complete();

    /** Abort the request. Any pending or submitted processing or completion
     * handler is aborted, if it is not yet being executed.
     */
    void
    Abort();

    /** Get current completion result. */
    Result
    Get_completion_result();

    /** Check if the request was completed. */
    bool
    Is_completed() const;

    /** Add reference from user handle. */
    void
    Add_ref();

    /** Release reference from user handle. When last references is released,
     * request is automatically completed with currently set result.
     */
    void
    Release_ref();

private:

    /** Master request, which controls the execution of vehicle request. */
    Request::Ptr request;

    /** Completion handler. */
    Completion_handler completion_handler;

    /** Counter to track references from user handles. */
    std::atomic_int ref_count = { 0 };

    friend class Vehicle;
};

/** Vehicle request with specific payload. */
template <class Payload>
class Vehicle_request_spec: public Vehicle_request {
    DEFINE_COMMON_CLASS(Vehicle_request_spec, Vehicle_request)

public:

    /** Construct vehicle request with specific payload.
     * Template arguments:
     * - *Args* Arguments passed to payload constructor.
     */
    template<typename... Args>
    Vehicle_request_spec(Completion_handler completion_handler,
                     Request_completion_context::Ptr completion_ctx,
                     Args &&... args):
        Vehicle_request(completion_handler, completion_ctx),
        payload(std::forward<Args>(args)...)
    {}

    /** Handle of a specific vehicle request. It has pointer and dereference
     * semantics for payload access.
     */
    class Handle: public Vehicle_request::Handle {
    public:

        using Vehicle_request::Handle::Handle;

        /** Access payload using pointer semantics. */
        Payload*
        operator ->()
        {
            return &static_cast<Vehicle_request_spec&>(*vehicle_request).payload;
        }

        /** Access payload using pointer semantics. */
        const Payload*
        operator ->() const
        {
            return &static_cast<Vehicle_request_spec&>(*vehicle_request).payload;
        }

        /** Access payload using dereference semantics. */
        Payload&
        operator *()
        {
            return static_cast<Vehicle_request_spec&>(*vehicle_request).payload;
        }

        const Payload&
        operator *() const
        {
            return static_cast<Vehicle_request_spec&>(*vehicle_request).payload;
        }
    };

    /** Specific payload of the request. */
    Payload payload;
};

/** Specialization of request without payload. */
template<>
class Vehicle_request_spec<void>: public Vehicle_request {
    DEFINE_COMMON_CLASS(Vehicle_request_spec, Vehicle_request)
public:
    /** Constructor, forwards all argument to base class. */
    template<typename... Args>
    Vehicle_request_spec(Args &&... args):
        Vehicle_request(std::forward<Args>(args)...)
    {}
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VEHICLE_REQUEST_H_ */
