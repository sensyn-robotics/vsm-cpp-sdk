// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of ugcs::vsm::Vehicle_request.
 */
#include <ugcs/vsm/vehicle_request.h>

using namespace ugcs::vsm;

Vehicle_request::Handle::Handle()
{

}

Vehicle_request::Handle::Handle(Vehicle_request::Ptr vehicle_request):
        vehicle_request(vehicle_request)
{

}

Vehicle_request::Handle&
Vehicle_request::Handle::operator =(Vehicle_request::Result result)
{
    Assign_result(result);
    return *this;
}

Vehicle_request::Handle::operator bool() const
{
    return vehicle_request != nullptr &&
           !vehicle_request->request->Is_completed();
}

void
Vehicle_request::Handle::Assign_result(Result result)
{
    vehicle_request->Set_completion_result(result);
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
Vehicle_request::Set_completion_result(Result result)
{
    completion_handler.Set_args(result);
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
