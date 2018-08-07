// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Windows-specific implementation.
 */

#include <ugcs/vsm/serial_processor.h>
#include <ugcs/vsm/windows_file_handle.h>
#include <ugcs/vsm/windows_wstring.h>

using namespace ugcs::vsm;

namespace {

/** Serial port mode on Windows. */
class Windows_serial_mode: public Serial_processor::Stream::Mode {

public:

    Windows_serial_mode(const Mode& mode);

    void
    Fill_dcb(DCB& dcb);

    void
    Fill_commtimeouts(COMMTIMEOUTS& timeouts);
};

Windows_serial_mode::Windows_serial_mode(const Mode& mode) : Mode(mode)
{
}

void
Windows_serial_mode::Fill_dcb(DCB& dcb)
{
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = baud;
    dcb.fBinary = true;
    dcb.fParity = parity_check;
    dcb.ByteSize = char_size;
    dcb.Parity = parity_check ? (parity ? ODDPARITY : EVENPARITY) : NOPARITY;
    dcb.StopBits = stop_bit ? TWOSTOPBITS : ONESTOPBIT;
}

void
Windows_serial_mode::Fill_commtimeouts(COMMTIMEOUTS& timeouts)
{
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = read_timeout.count();
}

class Serial_file_handle: public internal::Windows_file_handle {
public:

    Serial_file_handle(
            HANDLE handle,
            const Serial_processor::Stream::Mode& mode,
            const std::string& name);

    /** Configure the descriptor based on current mode. */
    void
    Configure();

private:

    /** Mode of the device. */
    Windows_serial_mode mode;

    /** Name of the port. */
    std::string name;

};

Serial_file_handle::Serial_file_handle(
        HANDLE handle,
        const Serial_processor::Stream::Mode& mode,
        const std::string& name) :
            internal::Windows_file_handle(handle),
            mode(mode),
            name(name)
{

}

void
Serial_file_handle::Configure()
{
    COMMTIMEOUTS timeouts;
    mode.Fill_commtimeouts(timeouts);

    if (!SetCommTimeouts(handle, &timeouts)) {
        VSM_EXCEPTION(File_processor::Exception,
            "Failed to set timeouts (SetCommTimeouts): %s",
            Log::Get_system_error().c_str());
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    if (!GetCommState(handle, &dcb)) {
        VSM_EXCEPTION(File_processor::Exception,
            "Failed to get mode (GetCommState): %s",
            Log::Get_system_error().c_str());
    }

    mode.Fill_dcb(dcb);

    if (!SetCommState(handle, &dcb)) {
        DWORD error_code = GetLastError();
        std::string error = Log::Get_system_error();
        if (error_code == ERROR_FILE_NOT_FOUND) {
            /* Serial port node already removed. */
            LOG_WARNING("Serial port node lost: %s", name.c_str());
            VSM_EXCEPTION(File_processor::Not_found_exception,
                "Failed to apply mode (SetCommState): %s", error.c_str());
        }
        /* Assuming mode parameters are not valid. */
        VSM_EXCEPTION(Invalid_param_exception,
            "Failed to apply mode (SetCommState): %s", error.c_str());
    }
}

} /* anonymous namespace */

Serial_processor::Stream::Native_handle::Unique_ptr
Serial_processor::Open_native_handle(const std::string &port_name, const Stream::Mode &mode)
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

    HANDLE handle;

    try {
        handle = CreateFileW(Windows_wstring(name),
                GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED, 0);
    } catch (const Windows_wstring::Conversion_failure&) {
        VSM_EXCEPTION(File_processor::Exception,
                "Failed to convert file name to wide character string: %s",
                name.c_str());
    }

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

    auto serial_handle = std::make_unique<Serial_file_handle>(handle, mode, port_name);
    serial_handle->Configure();

    return std::move(serial_handle);
}

std::list<std::string>
Serial_processor::Enumerate_port_names()
{
    auto ret = std::list<std::string>();
    HKEY hKey = NULL;
    TCHAR caDevName[100], caPortName[100];
    DWORD dwDevNameSize, dwPortNameSize, dwType;

    try
    {
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm",
                         0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            for (auto i = 0; ; i++ ) {
                dwDevNameSize = sizeof(caDevName);
                dwPortNameSize = sizeof(caPortName);
                if (RegEnumValue(hKey, i, caDevName, &dwDevNameSize, NULL,
                                 &dwType, reinterpret_cast<LPBYTE>(caPortName),
                                 &dwPortNameSize ) == ERROR_SUCCESS)
                {
                    /* Assume all devices we are dealing with are connected
                     * via USB and appear in registry as /Device/USB* or
                     * something.
                     * This is due to bugs in serial comms of internal or
                     * legacy ports.
                     * E.g. "Intel(R) Active Management Technology - SOL"
                     * driver is known to mess up so badly that no other
                     * ports can be used once the intel port is opened.
                     * It breaks all vehicle detection and communication over
                     * serial ports.
                     * So, skip any internal serial ports.
                     * I.e. devices in form \Device\Serial<number>
                     */
                    if (strncmp(caDevName, "\\Device\\Serial", 14) != 0) {
                        ret.emplace_back(std::string(caPortName));
                    }
                } else {
                    break;
                }
            }
            RegCloseKey( hKey );
        }
    }
    catch (...)
    {
        if (hKey != NULL) {
            RegCloseKey(hKey);
        }
    }
    return ret;
}
