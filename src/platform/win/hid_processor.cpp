// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** Windows driver for HID devices. */

#ifndef VSM_DISABLE_HID

#include <ugcs/vsm/hid_processor.h>
#include <ugcs/vsm/windows_file_handle.h>
#include "vsm_hid.h"

using namespace ugcs::vsm;

namespace {

class Windows_handle: public Hid_processor::Stream::Native_handle {
public:

    Windows_handle(Hid_processor::Device_id device_id);

    ~Windows_handle();

    HANDLE
    Get_read_handle();

    HANDLE
    Get_write_handle();

    virtual void
    Set_output_report(Io_buffer::Ptr data, uint8_t report_id) override;

    virtual Io_buffer::Ptr
    Get_input_report(size_t report_size, uint8_t report_id) override;

private:
    /** Device handle (raw input API). */
    HANDLE handle;
    /** Device node file handles. */
    HANDLE file_handle, file_handle_read, file_handle_write;

    PHIDP_PREPARSED_DATA preparsed_data = nullptr;

    /** Returns handle of the found device, nullptr if not found. */
    void
    Find_device(Hid_processor::Device_id device_id);

    /** Check if the handle matched the device ID.
     * @param name Returned device name.
     */
    bool
    Check_device(HANDLE device, Hid_processor::Device_id device_id);
};

} /* anonymous namespace */

/* Windows_handle class. */

Windows_handle::Windows_handle(Hid_processor::Device_id device_id)
{
    Find_device(device_id);
    LOG("HID device opened: %s", name.c_str());
}

Windows_handle::~Windows_handle()
{
    CloseHandle(handle);
    CloseHandle(file_handle);
    /* Read/Write handles closed by stream. */

    if (preparsed_data) {
        HidD_FreePreparsedData(preparsed_data);
    }
}

HANDLE
Windows_handle::Get_read_handle()
{
    return  file_handle_read;
}

HANDLE
Windows_handle::Get_write_handle()
{
    return file_handle_write;
}

bool
Windows_handle::Check_device(HANDLE device,
                             Hid_processor::Device_id device_id)
{
    RID_DEVICE_INFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    if (GetRawInputDeviceInfo(device, RIDI_DEVICEINFO, &info, &size) ==
        static_cast<UINT>(-1)) {

        VSM_SYS_EXCEPTION("GetRawInputDeviceInfo failed");
    }
    if (info.dwType != RIM_TYPEHID) {
        return false;
    }
    if (info.hid.dwVendorId != device_id.Get_vendor_id() ||
        info.hid.dwProductId != device_id.Get_product_id()) {

        return false;
    }

    if (GetRawInputDeviceInfo(device, RIDI_DEVICENAME, nullptr, &size)) {
        VSM_SYS_EXCEPTION("GetRawInputDeviceInfo failed");
    }
    char name_buf[size];
    if (GetRawInputDeviceInfo(device, RIDI_DEVICENAME, name_buf, &size) ==
        static_cast<UINT>(-1)) {

        VSM_SYS_EXCEPTION("GetRawInputDeviceInfo failed");
    }
    file_name = std::string(name_buf, size);

    file_handle = CreateFile(file_name.c_str(), 0,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr,
                             OPEN_EXISTING,
                             0, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        VSM_SYS_EXCEPTION("CreateFile failed");
    }

    file_handle_read = CreateFile(file_name.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED, nullptr);
    if (file_handle_read == INVALID_HANDLE_VALUE) {
        VSM_SYS_EXCEPTION("CreateFile failed");
    }

    file_handle_write = CreateFile(file_name.c_str(), GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED, nullptr);
    if (file_handle_write == INVALID_HANDLE_VALUE) {
        VSM_SYS_EXCEPTION("CreateFile failed");
    }

    /* USB device string maximal size is 126 characters. */
    WCHAR buf[128];
    if (!HidD_GetManufacturerString(file_handle, buf, sizeof(buf))) {
        VSM_SYS_EXCEPTION("HidD_GetManufacturerString failed");
    }
    char char_buf[128];
    size = WideCharToMultiByte(CP_ACP, 0, buf, -1, char_buf, sizeof(char_buf),
                               nullptr, nullptr);
    name = std::string(char_buf, size - 1);
    if (!HidD_GetProductString(file_handle, buf, sizeof(buf))) {
        VSM_SYS_EXCEPTION("HidD_GetProductString failed");
    }
    name += " - ";
    size = WideCharToMultiByte(CP_ACP, 0, buf, -1, char_buf, sizeof(char_buf),
                               nullptr, nullptr);
    name += std::string(char_buf, size - 1);

    if (!HidD_GetPreparsedData(file_handle, &preparsed_data)) {
        VSM_SYS_EXCEPTION("HidD_GetPreparsedData failed");
    }

    return true;
}

void
Windows_handle::Set_output_report(Io_buffer::Ptr data, uint8_t report_id)
{
    size_t len = data->Get_length();
    uint8_t buf[len + 1];
    buf[0] = report_id;
    memcpy(&buf[1], data->Get_data(), len);
    if (!HidD_SetOutputReport(file_handle, buf, len + 1)) {

        DWORD error = GetLastError();
        if (error == ERROR_INVALID_HANDLE ||
            error == ERROR_FILE_NOT_FOUND) {

            VSM_EXCEPTION(Hid_processor::Invalid_state_exception,
                          "HidD_SetOutputReport failed");
        }

        VSM_SYS_EXCEPTION("HidD_SetOutputReport failed");
    }
}

Io_buffer::Ptr
Windows_handle::Get_input_report(size_t report_size, uint8_t report_id)
{
    auto buf = std::make_shared<std::vector<uint8_t>>(report_size + 1);
    (*buf)[0] = report_id;
    if (!HidD_GetInputReport(file_handle, &buf->front(), report_size + 1)) {

        DWORD error = GetLastError();
        if (error == ERROR_INVALID_HANDLE ||
            error == ERROR_FILE_NOT_FOUND) {

            VSM_EXCEPTION(Hid_processor::Invalid_state_exception,
                          "HidD_GetInputReport failed");
        }

        VSM_SYS_EXCEPTION("HidD_GetInputReport failed");
    }
    return Io_buffer::Create(std::move(buf), 1);
}

void
Windows_handle::Find_device(Hid_processor::Device_id device_id)
{
    UINT num_devices = 0;
    if (GetRawInputDeviceList(nullptr, &num_devices, sizeof(RAWINPUTDEVICELIST))) {
        VSM_SYS_EXCEPTION("GetRawInputDeviceList failed");
    }
    if (!num_devices) {
        VSM_EXCEPTION(Hid_processor::Not_found_exception, "Device not found");
    }
    std::unique_ptr<RAWINPUTDEVICELIST> list
        (new RAWINPUTDEVICELIST[num_devices]);
    if (GetRawInputDeviceList(list.get(), &num_devices, sizeof(RAWINPUTDEVICELIST)) ==
        static_cast<UINT>(-1)) {

        VSM_SYS_EXCEPTION("GetRawInputDeviceList failed");
    }
    for (RAWINPUTDEVICELIST *dev = list.get(); dev < list.get() + num_devices; dev++) {
        if (dev->dwType == RIM_TYPEHID && Check_device(dev->hDevice, device_id)) {
            handle = dev->hDevice;
            return;
        }
    }
    VSM_EXCEPTION(Hid_processor::Not_found_exception, "Device not found");
}

Hid_processor::Stream::Ptr
Hid_processor::Create_stream(Device_id device_id)
{
    auto handle = std::make_shared<Windows_handle>(device_id);
    auto file_handle = std::make_unique<internal::Windows_file_handle>(
    		handle->Get_read_handle(),
    		handle->Get_write_handle());

    return std::make_shared<Stream>(
    		Shared_from_this(),
    		device_id,
            handle,
            std::move(file_handle));
}

Hid_processor::Stream::Native_handle::Ptr
Hid_processor::Stream::Native_handle::Create(Device_id device_id)
{
    return std::make_shared<Windows_handle>(device_id);
}

#endif /* VSM_DISABLE_HID */
