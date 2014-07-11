// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/vehicle_requests.h>

using namespace ugcs::vsm;

#include <UnitTest++.h>

/* To access Vehicle_request::request private member. */
namespace ugcs {
namespace vsm {
class Vehicle {
public:
    template<class Vehicle_request_ptr>
    static Request::Ptr
    Get_request(Vehicle_request_ptr req)
    {
        return req->request;
    }
};
}
}

typedef Vehicle_request_spec<std::string> Vsm_string_request;

struct Payload {
    std::string val;
};

typedef Vehicle_request_spec<Payload> Vsm_payload_string_request;

TEST(vehicle_request_handle_copy_and_payload_access)
{
    auto comp_handler = Make_dummy_callback<void, Vehicle_request::Result>();
    auto proc_handler = Make_dummy_callback<void>();
    auto comp_ctx = Request_completion_context::Create("UT vehicle request completion");
    comp_ctx->Enable();
    auto req = Vehicle_clear_all_missions_request::Create(comp_handler, comp_ctx);
    ugcs::vsm::Vehicle::Get_request(req)->Set_processing_handler(proc_handler);
    ugcs::vsm::Vehicle::Get_request(req)->Process(true);
    Vehicle_clear_all_missions_request::Handle h1(req);
    /* Initially NOK. */
    CHECK(Vehicle_request::Result::NOK == req->Get_completion_result());
    Vehicle_clear_all_missions_request::Handle h2;
    Vehicle_request::Handle baseh;
    /* Empty handle. */
    CHECK(!h2);
    CHECK(!baseh);
    CHECK(h1);
    /* Copy. */
    h2 = h1;
    baseh = h2;
    /* All are valid now. */
    CHECK(h1);
    CHECK(h2);
    CHECK(baseh);
    /* Set result via copied handle. */
    h2 = Vehicle_request::Result::OK;
    /* Propagated to the request itself. */
    CHECK(Vehicle_request::Result::OK == req->Get_completion_result());
    CHECK(!h1);
    CHECK(!h2);
    CHECK(!baseh);
    req->Abort();

    /* Payload access reference semantics. */
    auto req2 = Vsm_string_request::Create(comp_handler, comp_ctx);
    ugcs::vsm::Vehicle::Get_request(req2)->Set_processing_handler(proc_handler);
    ugcs::vsm::Vehicle::Get_request(req2)->Process(true);
    req2->payload = "42";
    Vsm_string_request::Handle h3(req2);
    CHECK("42" == *h3);
    CHECK(h3);
    baseh = h3;
    h3 = Vsm_string_request::Handle();
    CHECK(!h3);
    /* Base still keeps the reference. */
    CHECK(baseh);
    CHECK(!req2->Is_completed());
    /* Completion via base handle. */
    baseh = Vehicle_request::Result::OK;
    CHECK(req2->Is_completed());
    CHECK(Vehicle_request::Result::OK == req2->Get_completion_result());
    req2->Abort();

    /* Payload access pointer semantics. */
    auto req3 = Vsm_payload_string_request::Create(comp_handler, comp_ctx);
    ugcs::vsm::Vehicle::Get_request(req3)->Set_processing_handler(proc_handler);
    ugcs::vsm::Vehicle::Get_request(req3)->Process(true);
    req3->payload.val = "42";
    /* Automatic completion when last handle is destroyed. */
    {
        Vsm_payload_string_request::Handle h4(req3);
        {
            Vsm_payload_string_request::Handle h5(req3);
            CHECK("42" == h4->val);
            CHECK(h4);
            CHECK(h5);
        }
        CHECK(!req3->Is_completed());
        CHECK(h4);
    }
    CHECK(req3->Is_completed());
    CHECK(Vehicle_request::Result::NOK == req3->Get_completion_result());
    req3->Abort();

    comp_ctx->Disable();
}
