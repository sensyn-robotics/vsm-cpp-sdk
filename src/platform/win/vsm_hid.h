// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vsm_hid.h
 *
 * Windows HID API definition missing in MinGW.
 */

#ifndef HID_H_
#define HID_H_

#include <Hidsdi.h>
#include <Hidpi.h>

extern "C" {

// @{
/** Windows API methods. */
HIDAPI BOOLEAN NTAPI HidD_GetManufacturerString(
    HANDLE HidDeviceObject,
    PVOID Buffer,
    ULONG BufferLength
);

HIDAPI BOOLEAN NTAPI HidD_GetProductString(
    HANDLE HidDeviceObject,
    PVOID Buffer,
    ULONG BufferLength
);

HIDAPI BOOLEAN NTAPI HidD_GetFeature(
    HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
);

HIDAPI BOOLEAN NTAPI HidD_GetInputReport(
    HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
);

HIDAPI BOOLEAN NTAPI HidD_SetOutputReport(
    HANDLE HidDeviceObject,
    PVOID ReportBuffer,
    ULONG ReportBufferLength
);

HIDAPI BOOLEAN NTAPI HidD_GetPreparsedData(
    HANDLE HidDeviceObject,
    PHIDP_PREPARSED_DATA *PreparsedData
);

HIDAPI BOOLEAN NTAPI HidD_FreePreparsedData(
    PHIDP_PREPARSED_DATA PreparsedData
);

// @}

} /* extern "C" */

#endif /* HID_H_ */
