// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * XXX add comment
 */

#include <ugcs/vsm/vsm.h>
#include <ugcs/vsm/cucs_processor.h>
#include <ugcs/vsm/crash_handler.h>
#include <ugcs/vsm/service_discovery_processor.h>

#include <fstream>

using namespace ugcs::vsm;

namespace {

Properties::Ptr properties;

Timer_processor::Ptr timer_proc;
Cucs_processor::Ptr cucs_processor;
Socket_processor::Ptr socket_processor;
File_processor::Ptr file_processor;
Serial_processor::Ptr serial_processor;
Transport_detector::Ptr transport_detector;
Service_discovery_processor::Ptr discoverer;
#ifndef VSM_DISABLE_HID
Hid_processor::Ptr hid_processor;
#endif /* VSM_DISABLE_HID */

std::string properties_file;

} /* anonymous namespace */

void
ugcs::vsm::Initialize(int argc, char *argv[], const std::string &default_conf_file)
{
    std::string config_file = default_conf_file;
    for (auto c = 0; c < argc; c++) {
        if (strcmp(argv[c], "--config") == 0 && c < argc - 1) {
            c++;
            config_file = std::string(argv[c]);
        }
    }
    ugcs::vsm::Initialize(config_file);
}

void
ugcs::vsm::Initialize(const std::string &props_file)
{
    properties_file = props_file;
    properties = Properties::Get_instance();
    auto prop_stream = std::unique_ptr<std::fstream>
        (new std::fstream(properties_file, std::ios_base::in));
    if (!prop_stream->is_open()) {
        VSM_EXCEPTION(Invalid_param_exception, "Cannot open configuration file: %s",
                properties_file.c_str());
    }
    properties->Load(*prop_stream);

    if (    properties->Exists("log.level")
        &&  properties->Get("log.level").length()) {
        Log::Set_level(properties->Get("log.level"));
    }
    if (properties->Exists("log.file_path")) {
        Log::Set_custom_log(properties->Get("log.file_path"));
        Crash_handler::Set_reports_file_base(properties->Get("log.file_path") + "_crash_");
    }
    if (properties->Exists("log.single_max_size")) {
        Log::Set_max_custom_log_size(properties->Get("log.single_max_size"));
    }

    timer_proc = Timer_processor::Get_instance();
    timer_proc->Enable();

    socket_processor = Socket_processor::Get_instance();
    socket_processor->Enable();

    // transport_detector must initialize before cucs_processor because
    // cucs_processor calls transport_detector->Activate().
    transport_detector = Transport_detector::Get_instance();
    transport_detector->Enable();

    cucs_processor = Cucs_processor::Get_instance();
    cucs_processor->Enable();

    file_processor = File_processor::Get_instance();
    file_processor->Enable();

    serial_processor = Serial_processor::Get_instance();
    serial_processor->Enable();

    // Start service discovery protocol
    if (    properties->Exists("service_discovery.address")
        &&  properties->Exists("service_discovery.port")) {
        auto dicovery_listener = Socket_address::Create(
                properties->Get("service_discovery.address"),
                properties->Get("service_discovery.port"));
        if (!dicovery_listener->Is_multicast_address()) {
            VSM_EXCEPTION(
                    Invalid_param_exception,
                    "service_discovery.address '%s' is not a valid multicast address",
                    dicovery_listener->Get_address_as_string().c_str());
        }
        discoverer = Service_discovery_processor::Get_instance(dicovery_listener);
    } else {
        discoverer = Service_discovery_processor::Get_instance();
    }
    discoverer->Enable();

    /** Service discovery support.
     *
     * Support two syntaxes:
     * 1) single service: "service_discovery.advertise.name"
     * 2) multiple services: "service_discovery.advertise.<service_id>.name"
     * The value of service_id is ignored it is used only
     * for grouping name, type and location together.
     *
     * Example config entries:
     * service_discovery.advertise.1.name = Ardupilot VSM
     * service_discovery.advertise.1.type = ugcs:vsm:ardupilot
     * service_discovery.advertise.1.location = {local_address}:5556
     *
     */

    // Advertise configured services
    static std::string prefix = "service_discovery.advertise";
    for (auto base_it = properties->begin(prefix); base_it != properties->end(); base_it++) {
        try {
            std::string service_base(prefix + ".");
            if (base_it.Get_count() == 4) {
                service_base += base_it[2] + ".";
            }
            if (    *base_it == service_base + "name"
                &&  properties->Exists(service_base + "type")
                &&  properties->Exists(service_base + "location"))
            {
                discoverer->Advertise_service(
                        properties->Get(service_base + "type"),
                        properties->Get(service_base + "name"),
                        properties->Get(service_base + "location")
                        );
            }
        } catch (Exception&) {
            // ignore parsing errors.
        }
    }

#   ifndef VSM_DISABLE_HID
    hid_processor = Hid_processor::Get_instance();
    hid_processor->Enable();
#   endif /* VSM_DISABLE_HID */

}

void
ugcs::vsm::Terminate(bool save_config)
{
    discoverer->Disable();
    discoverer = nullptr;

    transport_detector->Disable();
    transport_detector = nullptr;

#   ifndef VSM_DISABLE_HID
    hid_processor->Disable();
    hid_processor = nullptr;
#   endif /* VSM_DISABLE_HID */

    cucs_processor->Disable();
    cucs_processor = nullptr;

    socket_processor->Disable();
    socket_processor = nullptr;

    file_processor->Disable();
    file_processor = nullptr;

    serial_processor->Disable();
    serial_processor = nullptr;

    timer_proc->Disable();
    timer_proc = nullptr;

    if (save_config && properties_file.size()) {
        auto prop_stream = std::unique_ptr<std::fstream>
            (new std::fstream(properties_file, std::ios_base::out | std::ios_base::trunc ));
        if (!prop_stream->is_open()) {
            VSM_EXCEPTION(Invalid_param_exception, "Cannot open configuration file for writing: %s",
                    properties_file.c_str());
        }
        properties->Store(*prop_stream);
    }
    properties = nullptr;
}
