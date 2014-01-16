// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file run_as_service.h
 *
 *  Supports Windows service creation/deletion and starting/stopping the service
 * User needs only one function call to Run_as_service()
 * to enable Windows service functionality.
 * In linux build Run_as_service() does nothing and returns false.
 */

#ifndef WINDOWS_SERVICE_H_
#define WINDOWS_SERVICE_H_

#include <vsm/callback.h>

namespace vsm {

/** Signature for program initialization routine.
 * int Program_init(int, char **, ...);
 * This function is called from the service main thread
 * when program is run as a service.
 * The function should be used to initialize the service functionality
 * and should return within reasonable time - less than 10 seconds.
 * Return result:
 *  0:        Tells that service initialized correctly.
 *            Service is reported as RUNNING to sc manager.
 *  non-zero: Service initialization failed.
 *            Service is reported as STOPPED to sc manager.
 */
typedef Callback_proxy<int, int, char **> Program_init;

/** Builder for init handler. */
DEFINE_CALLBACK_BUILDER(
        Make_program_init_handler,
        (int, char **),
        (0, nullptr))

/** Programs' deinit routine.
 * Called when service is about to get stopped.
 * Use this routine to cleanup resources.
 */
typedef Callback_proxy<void> Program_done;

/** Entry point for running program as a service.
 * This function should be the first called from program's main().
 * argc and argv from main() should be passed unchanged to the function.
 *
 * The program must be run as Administrator for --service commands to succeed.
 *
 * When launched from command line (not as a service) this function recognizes
 * the following arguments from argv array:
 * --service create|delete|start|stop|restart [--service-startup-mode auto|manual] \
 *     --service-description "Quoted text describing the service"
 * --config \<path_to_vsm_config_file\>
 *
 * -service create
 *  1) Creates the service with given name and sets startup mode according
 *     to "--service-startup-mode" argument. Starts the service if startup mode is set to auto.
 *  2) Saves the rest of given parameters in registry
 *     (excluding the "-service \<action\>" and "--service-startup-mode \<mode\>").
 *  3) --config \<file\> is treated in a special way:
 *     a) "vsm.conf" is used when --config parameter not present.
 *     b) File name is converted to absolute path.
 *  4) If config file is not accessible the service is not created and the call fails.
 *
 * --service delete
 *  Stops and removes the service from registry.
 *  (The same as running Windows command 'sc delete \<service_name\>')
 *
 * --service start
 *  Starts the service
 *  (The same as running Windows command 'sc start \<service_name\>')
 *
 * --service stop
 *  Stop the service
 *  (The same as running Windows command 'sc stop \<service_name\>')
 *
 * --service restart
 *  Stop and start the service. Used when config has changed.
 *  (The same as running Windows commands 'sc stop \<service_name\>'
 *  and then 'sc start \<service_name\>')
 *  To run the service with different config file path it must be reinstalled
 *  using -service delete and -service create calls
 *
 * --service state
 *  Display current state of service:
 *  Running, Stopped, ...
 *
 * --service-startup-mode manual (default)
 *  Used together with "--service create" command
 *  Service will require manual start with 'sc start \<service_name\>' command or
 *  '\<program\> --service start' command. This is the default mode when --service-startup-mode
 *  parameter not given.
 *
 * --service-startup-mode auto
 *  Used together with "--service create" command
 *  Start service automatically when Windows starts
 *
 *  --service-description "quoted string"
 *  Used to specify service description.
 *  This text will appear in Windows Service manager console
 *
 *  --service-account "quoted string"
 *  Used to specify account which will run the service.
 *
 *  --service-password "quoted string"
 *  Used to specify password for service account.
 *
 * When the program main is invoked as a service then Run_as_windows_service()
 * returns only when the service is stopped. The return value is true.
 *
 * @param service_name Name of the service this program will implement.
 * @param argc Parameter count.
 * @param argv Parameter string values (ANSI only, localized strings not supported)
 * @param prog_init Function to call on service startup (required)
 * @param prog_done Function to call on service stop. Can be NULL.
 *
 * @return true - service specific parameters were found and handled successfully.
 *                when run as service, the program is going to stop.
 *                Basically, your main function should exit right after this call returned true.
 *         false - no service specific parameters found and not invoked as service
 *                 In this case your program can continue as normal console app.
 */
bool
Run_as_service(const char* service_name, int argc, char *argv[], Program_init prog_init, Program_done prog_done);

} //namespace vsm
#endif /* WINDOWS_SERVICE_H_ */
