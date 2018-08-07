// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of ugcs::vsm::Vehicle_request.
 */
#include <ugcs/vsm/vehicle_request.h>
#include <vector>

using namespace ugcs::vsm;

Vehicle_request::Handle::Handle()
{
}

Vehicle_request::Handle::Handle(Vehicle_request::Ptr vehicle_request):
        vehicle_request(vehicle_request)
{
}

Vehicle_request::Handle::operator bool() const
{
    return vehicle_request != nullptr &&
           !vehicle_request->request->Is_completed();
}

void
Vehicle_request::Handle::Assign_result(Result result, const std::string& status_text)
{
    vehicle_request->Set_completion_result(result, status_text);
    vehicle_request->Complete();
}

Vehicle_request::Vehicle_request(
        Completion_handler completion_handler,
        Request_completion_context::Ptr completion_ctx):
                completion_handler(completion_handler)
{
    request = Request::Create();
    request->Set_completion_handler(completion_ctx, completion_handler);
    /* By default, completed with error. Desirable completion result should be
     * set explicitly by the SDK user via the handle. */
    completion_handler.Set_args(Result::NOK);
}

Vehicle_request::~Vehicle_request()
{
}

void
Vehicle_request::Handle::Fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Fail_v(format, args);
    va_end(args);
}

void
Vehicle_request::Handle::Fail_v(const char *format, va_list fmt_args)
{
    if (vehicle_request == nullptr || vehicle_request->request->Is_completed()) {
        return;
    }

    if (format) {
        va_list fmt_copy;
        va_copy(fmt_copy, fmt_args);
        int size = vsnprintf(nullptr, 0, format, fmt_args);
        auto buf = std::unique_ptr<std::vector<char>>(new std::vector<char>(size + 1));
        vsnprintf(&buf->front(), size + 1, format, fmt_copy);
        va_end(fmt_copy);
        Assign_result(Result::NOK, &buf->front());
    } else {
        Assign_result(Result::NOK);
    }
}

void
Vehicle_request::Handle::Fail(const std::string& reason)
{
    if (vehicle_request && !vehicle_request->request->Is_completed()) {
        Assign_result(Result::NOK, reason);
    }
}

void
Vehicle_request::Handle::Succeed()
{
    if (vehicle_request && !vehicle_request->request->Is_completed()) {
        Assign_result(Result::OK);
    }
}

void
Vehicle_request::Set_completion_result(Result result, const std::string& text)
{
    completion_handler.Set_args(result, text);
}

void
Vehicle_request::Complete()
{
    request->Complete();
}

void
Vehicle_request::Abort()
{
    request->Abort();
}

Vehicle_request::Result
Vehicle_request::Get_completion_result()
{
    return completion_handler.template Get_arg<0>();
}

bool
Vehicle_request::Is_completed() const
{
    return request->Is_completed();
}

void
Vehicle_request::Add_ref()
{
    atomic_fetch_add(&ref_count, 1);
}

void
Vehicle_request::Release_ref()
{
    int res = atomic_fetch_sub(&ref_count, 1);
    if (res <= 0) {
        VSM_EXCEPTION(Internal_error_exception, "Reference counter underflow");
    } else if (res == 1 && !Is_completed()) {
        /* User didn't complete the request explicitly. Might be user
         * error or intentional action. Make sure request is always
         * completed.
         */
        ASSERT(Get_completion_result() == Result::NOK);
        Complete();
    }
}
