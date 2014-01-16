// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/vsm.h>

/** [Callback function] */
/* Target function for callback. */
int
Sample_callback_function(int arg)
{
    LOG("Sample callback function, arg: %d", arg);
    return arg + 10;
}
/** [Callback function] */

/** [Class with callback method] */
class Sample_class {
public:
    int
    Sample_callback_method(int arg)
    {
        LOG("Sample callback method, arg: %d", arg);
        return arg + 10;
    }
};
/** [Class with callback method] */

/** [Callable class] */
class Callable_class {
public:
    int x;

    Callable_class(int x):
        x(x)
    {}

    int
    operator ()(int y)
    {
        LOG("Sample callback method, arg: %d", y);
        return x + y;
    }
};
/** [Callable class] */

/* Callbacks creation example. */
void
Simple_callbacks()
{
    /** [Create function callback] */
    auto func_cbk = vsm::Make_callback(Sample_callback_function, 10);
    /** [Create function callback] */

    /** [Call function callback] */
    LOG("Callback call result: %d", func_cbk());
    /** [Call function callback] */


    /** [Create method callback] */
    Sample_class class_instance;
    auto method_cbk = vsm::Make_callback(&Sample_class::Sample_callback_method,
                                         &class_instance, 10);
    LOG("Callback call result: %d", method_cbk());
    /** [Create method callback] */

    /** [Create callable class callback] */
    Callable_class callable_instance(10);
    auto callable_cbk = vsm::Make_callback(callable_instance, 10);
    LOG("Callback call result: %d", callable_cbk());
    /** [Create callable class callback] */

    /** [Create lambda callback] */
    auto lambda_cbk = vsm::Make_callback(
        [](int arg)
        {
            LOG("Sample lambda callback, arg: %d", arg);
            return arg + 10;
        },
        10);
    LOG("Callback call result: %d", lambda_cbk());
    /** [Create lambda callback] */
}

/** [Declare handler type] */
typedef vsm::Callback_proxy<int, double> Sample_handler_type;
/** [Declare handler type] */

/** [Sample api method] */
void
Some_api_method(Sample_handler_type handler)
{
    LOG("Callback result: %d", handler(20));
}
/** [Sample api method] */

/** [Sample callback builder] */
DEFINE_CALLBACK_BUILDER(Sample_handler_builder, (double), (3.14))
/** [Sample callback builder] */

/* Illustration for using vsm::Callback_proxy class */
void
Callback_proxies()
{
    /** [Sample api call] */
    auto My_callback_target = [](double enforced_arg, int user_arg)
        {
            LOG("Callback called, enforced argument %f user argument %d",
                enforced_arg, user_arg);
            return 30;
        };
    Some_api_method(Sample_handler_builder(My_callback_target, 10));
    /** [Sample api call] */
}

void
Requests_and_contexts()
{
    /** [Create sample contexts] */
    /* Create waiter object for our contexts. */
    vsm::Request_waiter::Ptr waiter = vsm::Request_waiter::Create();
    /* Create processor for requests. */
    vsm::Request_processor::Ptr processor = vsm::Request_processor::Create("Processor", waiter);
    /* Create completion context for notifications processing. */
    vsm::Request_completion_context::Ptr comp_ctx =
            vsm::Request_completion_context::Create("Completion context", waiter);
    /** [Create sample contexts] */

    /** [Enable sample contexts] */
    processor->Enable();
    comp_ctx->Enable();
    /** [Enable sample contexts] */

    /** [Create sample request] */
    vsm::Request::Ptr req = vsm::Request::Create();
    req->Set_processing_handler(
        vsm::Make_callback(
            [](int arg, vsm::Request::Ptr req)
            {
                LOG("Processing handler called, arg %d", arg);
                req->Complete();
            }, 10, req));
    req->Set_completion_handler(comp_ctx,
        vsm::Make_callback(
            [](int arg)
            {
                LOG("Completion notification handler called, arg %d", arg);
            }, 20));
    /** [Create sample request] */

    /** [Submit sample request] */
    processor->Submit_request(req);
    LOG("Request submitted");
    /** [Submit sample request] */

    /** [Run sample processor] */
    LOG("Before processor running");
    waiter->Wait_and_process({processor});
    LOG("After processor running");
    /** [Run sample processor] */

    /** [Run sample completion context] */
    LOG("Before completion context running");
    waiter->Wait_and_process({comp_ctx});
    LOG("After completion context running");
    /** [Run sample completion context] */

    /** [Disable sample contexts] */
    processor->Disable();
    comp_ctx->Disable();
    /** [Disable sample contexts] */

    /** [Create sample worker] */
    /* Demonstrate contexts serving in a separated thread. */
    vsm::Request_worker::Ptr worker = vsm::Request_worker::Create("Another thread",
        std::initializer_list<vsm::Request_container::Ptr>{processor, comp_ctx});
    /* Enable all attached containers. */
    worker->Enable_containers();
    /* Enable the worker. This will launch its thread. */
    worker->Enable();
    /** [Create sample worker] */

    /** [Create and submit sample request for worker] */
    req = vsm::Request::Create();
    req->Set_processing_handler(
        vsm::Make_callback(
            [](int arg, vsm::Request::Ptr req)
            {
                LOG("Processing handler called, arg %d", arg);
                req->Complete();
            }, 10, req));
    req->Set_completion_handler(comp_ctx,
        vsm::Make_callback(
            [](int arg)
            {
                LOG("Completion notification handler called, arg %d", arg);
            }, 20));
    LOG("Before request submission");
    processor->Submit_request(req);
    /* Give a chance to process the request in a parallel thread. */
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LOG("After request submission");

    /* Disable attached containers and the worker itself. */
    worker->Disable_containers();
    worker->Disable();
    /** [Create and submit sample request for worker] */
}

/** [Declare custom processor] */
class Sample_processor: public vsm::Request_processor {
    DEFINE_COMMON_CLASS(Sample_processor, vsm::Request_processor)
public:
    /* Type for result handler. */
    typedef vsm::Callback_proxy<void, double> Handler;
    /* Builder for handler. */
    DEFINE_CALLBACK_BUILDER(Make_handler, (double), (3.14));

    Sample_processor() : vsm::Request_processor("Sample processor") {}

    /* The method for accessing processor provided services. */
    vsm::Operation_waiter
    Sample_api_method(/* Some request parameter. */
                      int param,
                      /* Result handler. Default one does nothing thus discarding
                       * the result.
                       */
                      Handler handler = vsm::Make_dummy_callback<void, double>(),
                      /* Completion context for result handler invocation. Default
                       * value will use processor context.
                       */
                      vsm::Request_completion_context::Ptr comp_ctx = nullptr);

private:
    /* Default completion context if the caller does not provide own one. */
    vsm::Request_completion_context::Ptr def_comp_ctx;
    /* Worker with dedicated thread for this processor. */
    vsm::Request_worker::Ptr worker;

    /* Request processing handler. It is always invoked in the processor dedicated thread. */
    void
    Process_api_call(int param, vsm::Request::Ptr request, Handler handler);

    /* Called when the processor is enabled. */
    virtual void
    On_enable() override;

    /* Called when the processor is enabled. */
    virtual void
    On_disable() override;
};
/** [Declare custom processor] */

/** [Define processor enable disable] */
void
Sample_processor::On_enable()
{
    vsm::Request_processor::On_enable();
    def_comp_ctx = vsm::Request_completion_context::Create("Completion context");
    def_comp_ctx->Enable();
    worker = vsm::Request_worker::Create("Worker",
        std::initializer_list<vsm::Request_container::Ptr>{Shared_from_this(), def_comp_ctx});
    worker->Enable();
}

void
Sample_processor::On_disable()
{
    Set_disabled();
    worker->Disable();
    def_comp_ctx->Disable();
    def_comp_ctx = nullptr;
    worker = nullptr;
}
/** [Define processor enable disable] */

/** [Define sample api call] */
vsm::Operation_waiter
Sample_processor::Sample_api_method(int param,
                                    Handler handler,
                                    vsm::Request_completion_context::Ptr comp_ctx)
{
    vsm::Request::Ptr req = vsm::Request::Create();
    req->Set_processing_handler(
        vsm::Make_callback(&Sample_processor::Process_api_call, Shared_from_this(),
                           param, req, handler));
    req->Set_completion_handler(comp_ctx ? comp_ctx : def_comp_ctx,
                                handler);
    Submit_request(req);
    return req;
}
/** [Define sample api call] */

/** [Define sample request processing implementation] */
void
Sample_processor::Process_api_call(int param, vsm::Request::Ptr request,
                                   Handler handler)
{
    auto lock = request->Lock();
    if (!request->Is_processing()) {
        /* It might be canceled. */
        return;
    }
    handler.Set_args(param * 2);
    request->Complete(vsm::Request::Status::OK, std::move(lock));
}
/** [Define sample request processing implementation] */

void
Custom_processor()
{
    /** [Use custom processor] */
    Sample_processor::Ptr processor = Sample_processor::Create();
    processor->Enable();

    auto handler = Sample_processor::Make_handler(
        [](double result, int user_param)
        {
            LOG("Request completed, result %f, user param %d", result, user_param);
        }, 30);
    processor->Sample_api_method(10, handler);
    /* Give a chance to process the request in a parallel thread. */
    std::this_thread::sleep_for(std::chrono::seconds(1));

    processor->Disable();
    /** [Use custom processor] */
}

int
main (int argc, char *argv[])
{
    /** [Initialize] */
    vsm::Initialize(argc, argv);
    /** [Initialize] */

    Simple_callbacks();

    Callback_proxies();

    Requests_and_contexts();

    Custom_processor();

    /** [Terminate] */
    vsm::Terminate();
    /** [Terminate] */

    return 0;
}
