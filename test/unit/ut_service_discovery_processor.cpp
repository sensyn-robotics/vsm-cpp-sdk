// Copyright (c) 2017, Smart Projects Holdings Ltd
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
            Socket_address::Create("239.198.46.46","11111"));
    auto discoverer2 = ugcs::vsm::Service_discovery_processor::Create(
            Socket_address::Create("239.198.46.46","11111"));
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

    auto detector = [&](std::string type, std::string, std::string loc, std::string id, bool)
    {
        if (type == service_type1) {
            LOG("Discovered service %s, loc=%s id=%s", type.c_str(), loc.c_str(), id.c_str());
            if (!got_result1) {
                detected1.set_value();
                got_result1 = true;
            }
        }
        if (type == service_type2) {
            LOG("Discovered service %s, loc=%s id=%s", type.c_str(), loc.c_str(), id.c_str());
            if (!got_result2) {
                detected2.set_value();
                got_result2 = true;
            }
        }
    };

    // Test Deactivate functionality.
    discoverer1->Advertise_service(service_type1, "n", "l");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    discoverer1->Unadvertise_service(service_type1, "n", "l");

    discoverer1->Subscribe_for_service(service_type1, Service_discovery_processor::Make_detection_handler(detector), proc_context);

    // Make sure subscriber1 get response from noify, not from m-search.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //  Test subscribe and advertise.
    discoverer1->Advertise_service(service_type1, "name1", "tcp://{local_address}/path");
    discoverer2->Advertise_service(service_type2, "name2", "tcp://location1");

    // Make sure subscriber2 get response from m-search, not from notify.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    discoverer2->Subscribe_for_service(service_type2, Service_discovery_processor::Make_detection_handler(detector), proc_context);

    // Wait for 5 seconds to discover the services.
    // Normally it happnens much faster, but on our buildserver under valgrind it takes ~3 seconds.
    auto ret = detected1.get_future().wait_for(std::chrono::seconds(5));
    CHECK (ret == std::future_status::ready);
    if (ret == std::future_status::ready) {
        ret = detected2.get_future().wait_for(std::chrono::seconds(5));
        CHECK (ret == std::future_status::ready);
    }
    discoverer2->Disable();
    discoverer1->Disable();
    proc_context->Disable();
    worker->Disable();
    sp->Disable();
    tp->Disable();
}
