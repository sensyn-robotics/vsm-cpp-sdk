// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vsm.h
 *
 * Main include file for VSM SDK.
 */

#ifndef VSM_H_
#define VSM_H_

#include <vsm/vehicle.h>
#include <vsm/properties.h>
#include <vsm/serial_processor.h>
#include <vsm/socket_processor.h>
#include <vsm/hid_processor.h>
#include <vsm/mavlink_stream.h>
#include <vsm/actions.h>
#include <vsm/transport_detector.h>
#include <vsm/telemetry_manager.h>
#include <vsm/optional.h>

#include <ios>

/** All VSM SDK functionality resides in this namespace. */
namespace vsm
{

/** Initialize VSM SDK. This function should be called before using any API.
 *
 * @param props_file Name of the configuration file, 'vsm.conf' is the default.
 * @param props_open_mode Mode for configuration file opening.
 *
 * @throws Invalid_param_exception if configuration file cannot be opened.
 * @throws Properties::Parse_exception if the configuration file has invalid format.
 * @throws Properties::Not_found_exception if required entries are missing in
 *      the configuration file.
 * @throws Properties::Not_convertible_exception if configuration entries has
 *      invalid format (e.g. invalid string where integer value is required).
 */
void
Initialize(const std::string &props_file = "vsm.conf",
           std::ios_base::openmode props_open_mode = std::ios_base::in);

/** Initialize VSM SDK. This function should be called before using any API.
 * This function parses the argument list and initializes vsm according to them.
 * Supported arguments:
 *
 * --config \<confguration file name\>
 *
 * @param argc argument count in argv array
 * @param argv argument list
 *
 * @throws Invalid_param_exception if configuration file cannot be opened.
 * @throws Properties::Parse_exception if the configuration file has invalid format.
 * @throws Properties::Not_found_exception if required entries are missing in
 *      the configuration file.
 * @throws Properties::Not_convertible_exception if configuration entries has
 *      invalid format (e.g. invalid string where integer value is required).
 */
void
Initialize(int argc, char *argv[]);

/** Terminate VSM SDK. This function should be called when VSM is termination.
 * VSM SDK API cannot be used after that.
 *
 * @param save_config Indicates that VSM configuration file should be saved so
 *      that changes done in run-time will be persisted.
 */
void
Terminate(bool save_config = false);

}//namespace vsm

#endif /* VSM_H_ */
