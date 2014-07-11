// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** Linux driver for raw input device. */

#ifndef VSM_DISABLE_HID

#include <ugcs/vsm/hid_processor.h>

using namespace ugcs::vsm;

Hid_processor::Stream::Ptr
Hid_processor::Create_stream(Device_id device_id __UNUSED)
{
    //XXX
    return Stream::Ptr();
}

Hid_processor::Stream::Native_handle::Ptr
Hid_processor::Stream::Native_handle::Create(Device_id device_id __UNUSED)
{
    //XXX
    return Ptr();
}

//XXX

#endif /* VSM_DISABLE_HID */
