// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Unix-specific implementation.
 */

#include <ugcs/vsm/serial_processor.h>
#include <cstring>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

std::list<std::string>
ugcs::vsm::Serial_processor::Enumerate_port_names()
{
    int n;
    struct dirent **name_list;
    auto com_list = std::list<std::string>();

    /**
     * Scan through all the devices matching "/dev/cu.*"
     *
     * Empirically learned that all our USB serial port dongles
     * appear as devices in form "/dev/cu.usb*".
     * Looks like we can enumerate the ports
     * without all the heavy magic of Apples IOKit.
     *
     * Quote from http://pbxbook.com/other/mac-tty.html:
     * "
     * You might notice that each serial device shows up twice in /dev,
     * once as a tty.* and once as a cu.*. So, what's the difference? Well,
     * TTY devices are for calling into UNIX systems, whereas CU (Call-Up)
     * devices are for calling out from them (eg, modems). We want to call-out
     * from our Mac, so /dev/cu.* is the correct device to use.
     *
     * The technical difference is that /dev/tty.* devices will wait
     * (or listen) for DCD (data-carrier-detect), eg, someone calling in,
     * before responding. /dev/cu.* devices do not assert DCD, so they will
     * always connect (respond or succeed) immediately.
     * "
     *
     */
    const char* sysdir = "/dev/";
    auto Device_is_serial = [](const struct dirent * e) -> int
    {
        return (strncmp("cu.", e->d_name, 3) == 0);
    };
    n = scandir(sysdir, &name_list, Device_is_serial, NULL);
    if (n > 0)
    {
        while (n--)
        {
            auto devname = std::string(sysdir) + std::string(name_list[n]->d_name);
            com_list.push_back(devname);
            free(name_list[n]);
        }
        free(name_list);
    }
    // Return the list of detected comports
    return com_list;
}
