// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Windows-specific implementation.
 */

#include <vsm/serial_processor.h>

#include <windows.h>

using namespace vsm;

namespace {

/** Envelope for Windows file handle value. */
class Windows_file_handle_envelope:
    public File_processor::Stream::Native_handle::Envelope {
public:
    Windows_file_handle_envelope(HANDLE handle): handle(handle) {}

    virtual void *
    Get_handle() override
    {
        return &handle;
    }

private:
    /** File handle. */
    HANDLE handle;
};

} /* anonymous namespace */

std::unique_ptr<Serial_processor::Stream::Native_handle::Envelope>
Serial_processor::Stream::Open_handle(const std::string &port_name, const Mode &mode)
{
    /*  Prepend device prefix if not yet. */
    const char prefix[] = "\\\\.\\";
    std::string name;
    if ((port_name.size() >= sizeof(prefix) - 1) &&
        port_name.substr(0, sizeof(prefix) - 1) == prefix) {

        name = port_name;
    } else {
        name = prefix;
        name += port_name;
    }

    HANDLE handle = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                               0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
        case ERROR_ACCESS_DENIED:
            VSM_EXCEPTION(File_processor::Permission_denied_exception,
                          "Insufficient permissions for file opening: %s",
                          name.c_str());
        case ERROR_FILE_NOT_FOUND:
            VSM_EXCEPTION(File_processor::Not_found_exception,
                          "File not found: %s", name.c_str());
        default:
            VSM_EXCEPTION(File_processor::Exception,
                          "Failed to open file '%s': %s", name.c_str(),
                          Log::Get_system_error().c_str());
        }
    }

    /* Disable any timeouts so that we can have event-based workflow. */
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 1;
    if (!SetCommTimeouts(handle, &timeouts)) {
        CloseHandle(handle);
        VSM_EXCEPTION(File_processor::Exception,
                      "Failed to set timeouts (SetCommTimeouts): %s", Log::Get_system_error().c_str());
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        VSM_EXCEPTION(File_processor::Exception,
                      "Failed to get mode (GetCommState): %s", Log::Get_system_error().c_str());
    }

    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = mode.baud;
    dcb.fBinary = true;
    dcb.fParity = mode.parity_check;
    dcb.ByteSize = mode.char_size;
    dcb.Parity = mode.parity_check ? (mode.parity ? ODDPARITY : EVENPARITY) : NOPARITY;
    dcb.StopBits = mode.stop_bit ? TWOSTOPBITS : ONESTOPBIT;

    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        DWORD error_code = GetLastError();
        std::string error = Log::Get_system_error();
        if (error_code == ERROR_FILE_NOT_FOUND) {
            /* Serial port node already removed. */
            LOG_WARNING("Serial port node lost: %s", name.c_str());
            VSM_EXCEPTION(Not_found_exception,
                          "Failed to apply mode (SetCommState): %s", error.c_str());
        }
        /* Assuming mode parameters are not valid. */
        VSM_EXCEPTION(Invalid_param_exception,
                      "Failed to apply mode (SetCommState): %s", error.c_str());
    }

    return std::unique_ptr<Serial_processor::Stream::Native_handle::Envelope>
        (new Windows_file_handle_envelope(handle));
}

std::list<std::string>
Serial_processor::Enumerate_port_names()
{
    auto ret = std::list<std::string>();
    HKEY hKey;
    TCHAR caDevName[40], caPortName[20];
    DWORD dwDevNameSize, dwPortNameSize, dwType;

    try
    {
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm",
                         0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            for (auto i = 0; true; i++ )
            {
                dwDevNameSize = sizeof(caDevName);
                dwPortNameSize = sizeof(caPortName);
                if (RegEnumValue(hKey, i, caDevName, &dwDevNameSize, NULL,
                                 &dwType, reinterpret_cast<LPBYTE>(caPortName),
                                 &dwPortNameSize ) == ERROR_SUCCESS)
                {
                    ret.emplace_back(std::string(caPortName));
                }
                else
                    break;
            }
            RegCloseKey( hKey );
        }
    }
    catch (...)
    {
      if (hKey != NULL)
          RegCloseKey(hKey);
    }
    return ret;
}
