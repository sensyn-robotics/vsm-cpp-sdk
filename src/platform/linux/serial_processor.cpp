// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Unix-specific implementation.
 */

#include <ugcs/vsm/serial_processor.h>
#include <linux/serial.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace ugcs::vsm;

/** Maximum possible value for termios VMIN parameter. */
const uint8_t ugcs::vsm::Serial_processor::MAX_VMIN = 255;

std::list<std::string>
Serial_processor::Enumerate_port_names()
{
    int n;
    struct dirent **name_list;
    auto com_list = std::list<std::string>();
    const char* sysdir = "/sys/class/tty/";

    // Scan through /sys/class/tty - it contains all tty-devices in the system
    n = scandir(sysdir, &name_list, NULL, NULL);
    if (n > 0)
    {
        while (n--)
        {
            auto devdir = sysdir + std::string(name_list[n]->d_name);
            struct stat st;
            auto devicedir = devdir + "/device";

            if (lstat(devicedir.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
                // Stat the devdir and handle it if it is a symlink
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));

                // Append '/driver' and return basename of the target
                devicedir += "/driver";

                if (readlink(devicedir.c_str(), buffer, sizeof(buffer)) > 0) {
                    // Work only with devices with a driver
                    std::string driver(basename(buffer));
                    auto devfile = std::string("/dev/") + basename(devdir.c_str());

                    // serial8250-devices need another step (open) to verify existence
                    if (driver == "serial8250")
                    {
                        int fd = open(devfile.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
                        if (fd >= 0)
                        {
                            // Get serial_info
                            struct serial_struct serinfo;
                            /* Initialization to work-around Valgrind complains,
                             * it does not know TIOCGSERIAL ioctl and its side
                             * effects.
                             */
                            memset(&serinfo, 0, sizeof(serinfo));
                            if (ioctl(fd, TIOCGSERIAL, &serinfo) == 0)
                            {
                                // If device type is no PORT_UNKNOWN we accept the port
                                if (serinfo.type != PORT_UNKNOWN)
                                    com_list.push_back(devfile);
                            }
                            close(fd);
                        }
                    } else {
                        com_list.push_back(devfile);
                    }
                }
            }
            free(name_list[n]);
        }
        free(name_list);
    }
    // Return the list of detected comports
    return com_list;
}
