// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/param_setter.h>
#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/vsm.h>
#include <ugcs/vsm/callback.h>
#include <ugcs/vsm/debug.h>

#include <iostream>
#include <future>

#include <UnitTest++.h>
#include "../../include/ugcs/vsm/service_discovery_processor.h"

using namespace ugcs::vsm;
using namespace std;

class Test_case_wrapper
{
public:
    Test_case_wrapper() {
    }

    ~Test_case_wrapper() {
    }
};

TEST_FIXTURE(Test_case_wrapper, ssdp_processor)
{
    auto sp = ugcs::vsm::Socket_processor::Get_instance();
    auto discoverer1 = ugcs::vsm::Service_discovery_processor::Create(
            Socket_address::Create("239.198.1.1","11113"));
    auto discoverer2 = ugcs::vsm::Service_discovery_processor::Create(
            Socket_address::Create("239.198.1.1","11113"));
    auto tp = ugcs::vsm::Timer_processor::Get_instance();
    tp->Enable();

    auto proc_context = ugcs::vsm::Request_processor::Create("callback processor");
    proc_context->Enable();
    Request_worker::Ptr worker = Request_worker::Create(
            "UT processor worker",
            std::initializer_list<Request_container::Ptr>({proc_context}));

    std::string service_type1("Service1");
    std::string service_type2("Service2");
    worker->Enable();
    sp->Enable();
    discoverer1->Enable();
    discoverer2->Enable();

    std::promise<void> detected1;
    std::promise<void> detected2;

    bool got_result1 = false;
    bool got_result2 = false;

    auto detector = [&](std::string type, std::string, std::string loc, bool)
    {
        if (type == service_type1) {
            LOG("Discovered service %s, loc=%s", type.c_str(), loc.c_str());
            if (!got_result1) {
                detected1.set_value();
                got_result1 = true;
            }
        }
        if (type == service_type2) {
            LOG("Discovered service %s, loc=%s", type.c_str(), loc.c_str());
            if (!got_result2) {
                detected2.set_value();
                got_result2 = true;
            }
        }
    };

    // Test Deactivate functionality.
    discoverer1->Advertise_service(service_type1, "n", "l");
    discoverer1->Unadvertise_service(service_type1, "n", "l");

    //  Test subscribe and advertise.
    discoverer1->Advertise_service(service_type1, "name1", "tcp://{local_address}/path");
    discoverer2->Advertise_service(service_type2, "name2", "tcp://location1");

    discoverer1->Subscribe_for_service(service_type1, Service_discovery_processor::Make_detection_handler(detector), proc_context);
    discoverer2->Subscribe_for_service(service_type2, Service_discovery_processor::Make_detection_handler(detector), proc_context);

    auto ret = detected1.get_future().wait_for(std::chrono::seconds(2));
    CHECK (ret == std::future_status::ready);
    if (ret == std::future_status::ready) {
        ret = detected2.get_future().wait_for(std::chrono::seconds(2));
        CHECK (ret == std::future_status::ready);
    }
    discoverer2->Disable();
    discoverer1->Disable();
    proc_context->Disable();
    worker->Disable();
    sp->Disable();
    tp->Disable();
}
