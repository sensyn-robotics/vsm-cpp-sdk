// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** Linux driver for raw input device. */

#ifndef VSM_DISABLE_HID

#include <vsm/hid_processor.h>

using namespace vsm;

Hid_processor::Stream::Native_handle::Ptr
Hid_processor::Stream::Native_handle::Create(Device_id device_id __UNUSED)
{
    //XXX
    return Ptr();
}

//XXX

#endif /* VSM_DISABLE_HID */
