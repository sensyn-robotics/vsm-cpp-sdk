// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Unix-specific implementation.
 */

#include <vsm/serial_processor.h>
#include <vsm/log.h>

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <iostream>

using namespace vsm;

namespace {

/** Envelope for POSIX file descriptor value. */
class Posix_file_handle_envelope:
    public File_processor::Stream::Native_handle::Envelope {
public:
    Posix_file_handle_envelope(int fd): fd(fd) {}

    virtual void *
    Get_handle() override
    {
        return &fd;
    }

private:
    /** File descriptor. */
    int fd;
};

/** Map provided desired rate value to the closest baud rate constant. */
tcflag_t
Map_baud_rate(int baud)
{
    static struct Rate {
        /* Numeric value. */
        int value;
        /* Corresponding constant. */
        tcflag_t constant;
    } rates[] = {
        { 0,        B0 },
        { 50,       B50 },
        { 75,       B75 },
        { 110,      B110 },
        { 134,      B134 },
        { 150,      B150 },
        { 200,      B200 },
        { 300,      B300 },
        { 600,      B600 },
        { 1200,     B1200 },
        { 1800,     B1800 },
        { 2400,     B2400 },
        { 4800,     B4800 },
        { 9600,     B9600 },
        { 19200,    B19200 },
        { 38400,    B38400 },
        { 57600,    B57600 },
        { 115200,   B115200 },
        { 230400,   B230400 }
    };

    size_t i;
    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        if (baud <= rates[i].value) {
            break;
        }
    }
    if (i == 0 && baud != 0) {
        return rates[1].constant;
    }
    if (i == sizeof(rates) / sizeof(rates[0]) ||
        baud - rates[i - 1].value < rates[i].value - baud) {

        i--;
    }
    return rates[i].constant;
}

/** Map provided desired character size value to the corresponding constant. */
tcflag_t
Map_char_size(int size)
{
    switch (size) {
    case 5:
        return CS5;
    case 6:
        return CS6;
    case 7:
        return CS7;
    case 8:
        return CS8;
    default:
        VSM_EXCEPTION(Invalid_param_exception, "Not allowed character size value");
    }
}

} /* anonymous namespace */

std::unique_ptr<Serial_processor::Stream::Native_handle::Envelope>
Serial_processor::Stream::Open_handle(const std::string &port_name, const Mode &mode)
{
    int fd = open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        switch (errno) {
        case EACCES:
            VSM_EXCEPTION(File_processor::Permission_denied_exception,
                          "Insufficient permissions for file opening: %s",
                          port_name.c_str());
        case ENOENT:
            VSM_EXCEPTION(File_processor::Not_found_exception,
                         "File not found: %s", port_name.c_str());
        default:
            VSM_EXCEPTION(File_processor::Exception,
                          "Failed to open file '%s': %s", port_name.c_str(),
                          Log::Get_system_error().c_str());
        }
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        VSM_SYS_EXCEPTION("Failed to get flags");
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        VSM_SYS_EXCEPTION("Failed to set flags");
    }

    /* Set up mode. */
    struct termios tio;
    std::memset(&tio, 0, sizeof(tio));

    tio.c_iflag = IGNBRK;
    tio.c_cflag = Map_baud_rate(mode.baud) | Map_char_size(mode.char_size) |
        (mode.stop_bit ? CSTOPB : 0) | CREAD |
        (mode.parity_check ? PARENB : 0) | (mode.parity ? PARODD : 0);
    /* Blocking read until 1 character arrives. */
    tio.c_cc[VMIN] = 1;
    if (tcflush(fd, TCIFLUSH)) {
        VSM_SYS_EXCEPTION("tcflush() failed");
    }
    if (tcsetattr(fd, TCSANOW, &tio)) {
        VSM_SYS_EXCEPTION("tcsetattr() failed");
    }

    return std::unique_ptr<Serial_processor::Stream::Native_handle::Envelope>
        (new Posix_file_handle_envelope(fd));
}

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

            if (lstat(devicedir.c_str(), &st) == 0 && S_ISLNK(st.st_mode))
            {// Stat the devdir and handle it if it is a symlink
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));

                // Append '/driver' and return basename of the target
                devicedir += "/driver";

                if (readlink(devicedir.c_str(), buffer, sizeof(buffer)) > 0)
                {// Work only with devices with a driver
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
                    }
                    else
                        com_list.push_back(devfile);
                }
            }
            free(name_list[n]);
        }
        free(name_list);
    }
    // Return the list of detected comports
    return com_list;
}
