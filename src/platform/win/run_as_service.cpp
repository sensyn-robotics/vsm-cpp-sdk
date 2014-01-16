// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * windows_service.cpp
 *
 *  Created on: Aug 8, 2013
 *      Author: Janis
 */

#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

#ifdef ERROR
#undef ERROR
#endif

#include <vsm/log.h>

#include <vsm/run_as_service.h>

namespace {//anonymous namespace

// Windows service name. Set by main entry point Run_as_windows_service()
CHAR g_service_name[MAX_PATH];

// argument list is saved in registry under the key
// "SYSTEM\\CurrentControlSet\\Services\\" + g_service_name + "\\Parameters"
// in this value
static std::string arguments_reg_value("Arguments");

// argument list is saved as one string separated with this character:
const CHAR argument_separator = '|';

// Used to report service status to EventViewer
SERVICE_STATUS_HANDLE   svc_status_handle;

vsm::Program_init g_start_main;
vsm::Program_done g_stop_main;

// Report the current service status to the SCM.
VOID
ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;
    SERVICE_STATUS svc_status;

    // Fill in the SERVICE_STATUS structure.

    svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    svc_status.dwServiceSpecificExitCode = 0;

    svc_status.dwCurrentState = dwCurrentState;
    svc_status.dwWin32ExitCode = dwWin32ExitCode;
    svc_status.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        svc_status.dwControlsAccepted = 0;
    else svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ( (dwCurrentState == SERVICE_RUNNING) ||
           (dwCurrentState == SERVICE_STOPPED) )
        svc_status.dwCheckPoint = 0;
    else svc_status.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus( svc_status_handle, &svc_status );
}

// Log string into event log
VOID
Report_event(LPCTSTR data)
{
      char *strings[1];

      /* Open event log */
      HANDLE handle = RegisterEventSource(0, g_service_name);
      if (! handle) return;

      strings[0] = const_cast<LPTSTR>(data);
      ReportEvent(handle, EVENTLOG_INFORMATION_TYPE, 0, 13, 0, 1, 0, const_cast<const char **>(strings), 0);

      /* Close event log */
      DeregisterEventSource(handle);
}

// Service control handler
VOID WINAPI
Windows_service_ctrl_handler( DWORD dwCtrl )
{
    // Handle the requested control code.
    switch(dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        g_stop_main();
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;

    default:
        break;
    }
}

// Service main
VOID WINAPI
Windows_service_main(DWORD argc, LPTSTR* argv)
{
    // Register the handler function for the service

    svc_status_handle = RegisterServiceCtrlHandler(
            g_service_name,
            Windows_service_ctrl_handler);

    if( !svc_status_handle )
    {
        Report_event(TEXT("RegisterServiceCtrlHandler error"));
        return;
    }

    // Report initial status to the SCM

    ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 10000 );

    // Perform service-specific initialization and work.

    bool ret = false;
    LONG error = NO_ERROR;
    if (argc > 1) {
        // If more than one argument is specified then service is started
        // manually from command line with specific arguments
        // In that case we do not load arguments from registry and pass
        // whatever is given by the user ignoring params in registry
        ret = (g_start_main(argc, argv) == 0);
    } else {
        // If there are no arguments or only one (name of service binary)
        // then the service is invoked by scmanager or manually started
        // without any arguments
        // We create argument list from the string saved in registry via create service call.

        DWORD datalen = 1;
        DWORD datatype;
        std::vector<CHAR> reg_data(datalen);
        std::vector<CHAR*> args;
        HKEY param_key;

        auto reg_key = std::string("SYSTEM\\CurrentControlSet\\Services\\") +
                g_service_name +
                "\\Parameters";
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_key.c_str(), 0, KEY_READ, &param_key);
        if (error == NO_ERROR) {
            do {
                error = RegQueryValueEx(
                    param_key,
                    arguments_reg_value.c_str(),
                    NULL,                   // reserved
                    &datatype,              // get value type
                    reinterpret_cast<BYTE*>(reg_data.data()),
                    &datalen);
                reg_data.resize(datalen);       // resize vector to fit the string
            } while (error == ERROR_MORE_DATA);
        }

        if (error == NO_ERROR && datatype == REG_SZ) {
            // got paramstring from registry, now construct arglist to pass to main

            // First make sure the string is null terminated
            // RegQueryValueEx does not fix it if string was saved without nulltermintaor.
            reg_data.push_back('\0');
            CHAR* cur_param = reg_data.data();
            auto counter = 0;

            // add executable name as first argument
            if (argc > 0) {
                args.push_back(argv[0]);
                counter++;
            }

            // find the delimiters and create args vector with pointers into reg_data.
            for (auto &c : reg_data) {
                if (c == argument_separator) {
                    c = '\0';
                    counter++;
                    args.push_back(cur_param);
                    cur_param = (&c) + 1;
                }
            }

            // Launch program initialization code.
            ret = (g_start_main(counter, args.data()) == 0);

            LOG_INFO("Running as service \"%s\" with %d arguments", g_service_name, counter);
            auto c = 0;
            for (auto& it : args) {
                LOG_INFO("Service parameter [%d]=\"%s\"", c++, it);
            }
        } else {
            Report_event(TEXT("Error while retrieving parameters from registry"));
        }
    }

    // Report running status when initialization is complete.
    if (ret)
        ReportSvcStatus(SERVICE_RUNNING, error, 0);
    else
        ReportSvcStatus(SERVICE_STOPPED, error, 0);
}

// Helper function to Open Service Control Manager
SC_HANDLE
Open_scm()
{
    // Get a handle to the SCM database.
    auto sc_manager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database
        SC_MANAGER_ALL_ACCESS);  // full access rights

    if (NULL == sc_manager)
    {
        LOG_ERROR("OpenSCManager failed (%s)", vsm::Log::Get_system_error().c_str());
        return NULL;
    }
    return sc_manager;
}

// Helper function to Open Service Control Manager
SC_HANDLE
Open_service(SC_HANDLE sc_manager)
{
    if (sc_manager) {
        auto service = OpenService(sc_manager, g_service_name, SERVICE_ALL_ACCESS);
        if (service) {
            return service;
        } else {
            LOG_INFO("OpenService failed (%s)", vsm::Log::Get_system_error().c_str());
        }
    }
    return NULL;
}

bool
Windows_service_start()
{
    auto ret = false;
    auto sc_manager = Open_scm();
    if (sc_manager) {
        auto service = Open_service(sc_manager);
        if (service) {
            if (StartService(service, 0, NULL)) {
                ret = true;
            } else {
                LOG_INFO("StartService failed (%s)", vsm::Log::Get_system_error().c_str());
            }
            CloseServiceHandle(service);
        }
        CloseServiceHandle(sc_manager);
    }
    return ret;
}

bool
Windows_service_stop()
{
    auto ret = false;
    auto sc_manager = Open_scm();
    if (sc_manager) {
        auto service = Open_service(sc_manager);
        if (service) {
            SERVICE_STATUS ssp;
            ControlService(service, SERVICE_CONTROL_STOP, &ssp);
            auto err = GetLastError();
            if (    err == ERROR_SERVICE_NOT_ACTIVE
                ||  err == NO_ERROR) {
                ret = true;
            } else {
                LOG_INFO("StopService failed (%s)", vsm::Log::Get_system_error().c_str());
            }
            CloseServiceHandle(service);
        }
        CloseServiceHandle(sc_manager);
    }
    return ret;
}

bool
Windows_service_create(int argc, char *argv[])
{
    TCHAR szPath[MAX_PATH];
    LPTSTR description = nullptr;
    LPTSTR service_account = NULL;
    LPTSTR service_password = NULL;
    std::string service_account_string;
    auto startup_mode = SERVICE_DEMAND_START;

    if( !GetModuleFileName( NULL, szPath, MAX_PATH ) )
    {
        LOG_ERROR("GetModuleFileName failed (%s)", vsm::Log::Get_system_error().c_str());
        return false;
    }

    // process the parameters:
    // strip out the --service and --service-startup-mode flags
    // create default --config from current directory if not given
    // config file must be given with absolute path to wirk with service
    // if conf path is relative it is assumed to be relative to
    // current directory at the time of calling with "--service install"
    // save the resulting param string in service registry
    std::string conf_file;
    std::string param_str;
    CHAR buffer[MAX_PATH];
    for (auto c = 1; c < argc; c++) {
        auto save_param = true;
        if (strcmp(argv[c], "--config") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
                LPTSTR name;
                if (GetFullPathName(argv[c], MAX_PATH, buffer, &name) > 0) {
                    conf_file = std::string(buffer);
                }
            }
        }

        if (strcmp(argv[c], "--service-startup-mode") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
                if (strcmp(argv[c], "manual") == 0)
                    startup_mode = SERVICE_DEMAND_START;
                else if (strcmp(argv[c], "auto") == 0)
                    startup_mode = SERVICE_AUTO_START;
            }
        }

        if (strcmp(argv[c], "--service-account") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
                service_account = argv[c];
                if (strchr(service_account, '\\') == nullptr) {
                    /* Append ".\" to user name if domain not specified.
                     * Assume built-in domain. */
                    service_account_string = std::string(".\\") + service_account;
                    service_account = const_cast<LPTSTR>(service_account_string.c_str());
                }
            }
        }

        if (strcmp(argv[c], "--service-password") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
                service_password = argv[c];
            }
        }

        if (strcmp(argv[c], "--service") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
            }
        }

        if (strcmp(argv[c], "--service-description") == 0) {
            save_param = false;
            if (c < argc - 1) {
                c++;
                description = argv[c];
            }
        }

        if (save_param) {
            param_str += std::string(argv[c]);
            param_str += argument_separator;
        }
    }

    if (conf_file == "") {
        // conf file not specified. using default
        if (GetCurrentDirectory(MAX_PATH, buffer) > 0) {
            conf_file = std::string(buffer) + "\\vsm.conf";
        }
    }

    // Test if the config file exists and is readable
    auto f = fopen(conf_file.c_str(), "rt+");
    if (f) {
        fclose(f);
        LOG_INFO("Using config file '%s'", conf_file.c_str());
        param_str += std::string("--config") + argument_separator + conf_file + argument_separator;
    } else {
        LOG_ERROR("Failed to open config file '%s'", conf_file.c_str());
        return false;
    }

    auto sc_manager = Open_scm();
    if (NULL == sc_manager) {
        return false;
    }

    // Create the service
    auto my_service = CreateService(
        sc_manager,              // SCM database
        g_service_name,            // name of service
        g_service_name,            // service name to display
        SERVICE_ALL_ACCESS,        // desired access
        SERVICE_WIN32_OWN_PROCESS, // service type
        startup_mode,              // start type
        SERVICE_ERROR_NORMAL,      // error control type
        szPath,                    // path to service's binary
        NULL,                      // no load ordering group
        NULL,                      // no tag identifier
        NULL,                      // no dependencies
        service_account,           // LocalSystem account if NULL
        service_password);         // password

    if (my_service == NULL) {
        LOG_ERROR("CreateService failed (%s)", vsm::Log::Get_system_error().c_str());
        CloseServiceHandle(sc_manager);
        return false;
    }

    if (description) {
        SERVICE_DESCRIPTION description_struct = { description };
        // Do not allow exceedingly long descriptions.
        if (strlen(description) > 1000) {
            description[1000] = 0;
        }
        ChangeServiceConfig2(my_service, SERVICE_CONFIG_DESCRIPTION, &description_struct);
    }

    CloseServiceHandle(my_service);
    CloseServiceHandle(sc_manager);

    // create regkey only if service is created successfully.
    auto reg_key = std::string("SYSTEM\\CurrentControlSet\\Services\\") + g_service_name + "\\Parameters";
    HKEY hKey;
    if (RegCreateKeyEx(
            HKEY_LOCAL_MACHINE,
            reg_key.c_str(),
            0,
            NULL,
            0,
            KEY_WRITE,
            NULL,
            &hKey,
            NULL) == ERROR_SUCCESS)
    {
        RegSetValueEx(hKey, arguments_reg_value.c_str(), 0, REG_SZ,
                reinterpret_cast<const BYTE*>(param_str.c_str()), param_str.length() + 1);
        RegCloseKey( hKey );
        if (startup_mode == SERVICE_AUTO_START)
            return Windows_service_start();
    } else {
        LOG_ERROR("Failed to parameters in registry (%s)", vsm::Log::Get_system_error().c_str());
    }
    return true;
}

bool
Windows_service_delete()
{
    auto ret = false;
    auto sc_manager = Open_scm();
    if (sc_manager) {
        auto service = Open_service(sc_manager);
        if (service) {
            DeleteService(service);
            auto err = GetLastError();
            if (    err == ERROR_SERVICE_MARKED_FOR_DELETE
                ||  err == NO_ERROR) {
                ret = true;
            } else {
                LOG_INFO("DeleteService failed (%s)", vsm::Log::Get_system_error().c_str());
            }
            CloseServiceHandle(service);
        }
        CloseServiceHandle(sc_manager);
    }
    return ret;
}

bool
Windows_service_get_state()
{
    auto ret = false;
    auto sc_manager = Open_scm();
    if (sc_manager) {
        auto service = Open_service(sc_manager);
        if (service) {
            SERVICE_STATUS ssp;
            DWORD size;
            LOG_INFO("Service name= \"%s\"", g_service_name);
            if (    !QueryServiceConfig(service, NULL, 0, &size)
                &&  GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<CHAR> data(size);
                LPQUERY_SERVICE_CONFIG config = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(data.data());
                if (QueryServiceConfig(service, config, size, &size)) {
                    LOG_INFO("Service binary= \"%s\"", config->lpBinaryPathName);
                }
            }
            if (QueryServiceStatus(service, &ssp)) {
                switch (ssp.dwCurrentState)
                {
                case SERVICE_RUNNING:
                    LOG_INFO("Service is running");
                    break;
                case SERVICE_START_PENDING:
                    LOG_INFO("Service is starting");
                    break;
                case SERVICE_STOP_PENDING:
                    LOG_INFO("Service is stopping");
                    break;
                case SERVICE_STOPPED:
                    LOG_INFO("Service is stopped");
                    break;
                default:
                    LOG_INFO("Service state=%d", ssp.dwCurrentState);
                    break;
                }
                ret = true;
            } else {
                LOG_INFO("QueryServiceStatus failed (%s)", vsm::Log::Get_system_error().c_str());
            }
            CloseServiceHandle(service);
        }
        CloseServiceHandle(sc_manager);
    }
    return ret;
}
} //anonymous namespace

using namespace vsm;

// Main (and only) entry point for windows service framework
bool
vsm::Run_as_service(const char* service_name, int argc, char *argv[], Program_init prog_start, Program_done prog_stop)
{
    g_start_main = prog_start;
    g_stop_main = prog_stop;
    strncpy(g_service_name, service_name, MAX_PATH);

    /*
      Optimisation for Windows 2000:
      When we're run from the command line the StartServiceCtrlDispatcher() call
      will time out after a few seconds on Windows 2000.  On newer versions the
      call returns instantly.  Check for stdin first and only try to call the
      function if there's no input stream found.  Although it's possible that
      we're running with input redirected it's much more likely that we're
      actually running as a service.
      This will save time when running with no arguments from a command prompt.
    */
    if (_fileno(stdin) < 0)
    {
        SERVICE_TABLE_ENTRY dispatch_table[] =
        {
            { g_service_name, static_cast<LPSERVICE_MAIN_FUNCTION>(Windows_service_main) },
            { NULL, NULL }
        };
        // This call returns when the service has stopped.
        // The process should simply terminate when the call returns.
        if (StartServiceCtrlDispatcher(dispatch_table)) {
            return true;    // Was invoked as service main.
        } else {
            unsigned long error = GetLastError();
            if (error != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                Report_event(TEXT("StartServiceCtrlDispatcher error"));
                return true;
            }
        }
    }

    /* If we are here user ran it from a cmd line */
    for (auto c = 1; c < argc; c++)
    {
        if (strcmp(argv[c], "--service") == 0 && c < argc - 1)
        {
            c++;
            if (strcmp(argv[c], "create") == 0) {
                if (Windows_service_create(argc, argv)) {
                    LOG_INFO("Service %s created", g_service_name);
                }
            } else if (strcmp(argv[c], "delete") == 0) {
                if (Windows_service_stop() && Windows_service_delete())
                    LOG_INFO("Service %s deleted", g_service_name);
            } else if (strcmp(argv[c], "start") == 0) {
                if (Windows_service_start())
                    LOG_INFO("Service %s started", g_service_name);
            } else if (strcmp(argv[c], "stop") == 0) {
                if (Windows_service_stop())
                    LOG_INFO("Service %s stopped", g_service_name);
            } else if (strcmp(argv[c], "restart") == 0) {
                if (Windows_service_stop() && Windows_service_start())
                    LOG_INFO("Service %s restarted", g_service_name);
            } else if (strcmp(argv[c], "state") == 0) {
                Windows_service_get_state();
            } else {
                LOG_ERROR("Unknown service option");
            }
            return true;    // service option handled.
        }
    }
    // no service specific parameters given.

    // check for installed service.
    return false;   // no service specific parameters given.
}
