// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * XXX add comment
 */

#include <vsm/vsm.h>
#include <vsm/cucs_processor.h>
#include <vsm/crash_handler.h>

#include <fstream>

using namespace vsm;

namespace {

Properties::Ptr properties;
std::unique_ptr<std::fstream> properties_stream;

Timer_processor::Ptr timer_proc;
Cucs_processor::Ptr cucs_processor;
Socket_processor::Ptr socket_processor;
File_processor::Ptr file_processor;
Serial_processor::Ptr serial_processor;
Transport_detector::Ptr transport_detector;
#ifndef VSM_DISABLE_HID
Hid_processor::Ptr hid_processor;
#endif /* VSM_DISABLE_HID */

} /* anonymous namespace */

void
vsm::Initialize(int argc, char *argv[])
{
    std::string config_file("vsm.conf");
    for (auto c = 0; c < argc; c++) {
        if (strcmp(argv[c], "--config") == 0 && c < argc - 1) {
            c++;
            config_file = std::string(argv[c]);
        }
    }
    vsm::Initialize(config_file);
}

void
vsm::Initialize(const std::string &props_file,
                std::ios_base::openmode props_open_mode)
{
    properties = Properties::Get_instance();
    properties_stream = std::unique_ptr<std::fstream>
        (new std::fstream(props_file, props_open_mode));
    if (!properties_stream->is_open()) {
        VSM_EXCEPTION(Invalid_param_exception, "Cannot open configuration file: %s",
                      props_file.c_str());
    }
    properties->Load(*properties_stream);

    if (properties->Exists("log.level")) {
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

    cucs_processor = Cucs_processor::Get_instance();
    cucs_processor->Enable();

    file_processor = File_processor::Get_instance();
    file_processor->Enable();

    serial_processor = Serial_processor::Get_instance();
    serial_processor->Enable();

#   ifndef VSM_DISABLE_HID
    hid_processor = Hid_processor::Get_instance();
    hid_processor->Enable();
#   endif /* VSM_DISABLE_HID */

    transport_detector = Transport_detector::Get_instance();
    transport_detector->Enable();
}

void
vsm::Terminate(bool save_config)
{
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

    if (save_config) {
        properties->Store(*properties_stream);
    }
    properties_stream = nullptr;
    properties = nullptr;
}
