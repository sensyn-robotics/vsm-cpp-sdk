// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Serial_processor platform-specific part.
 * Unix-specific implementation.
 */

#include <ugcs/vsm/serial_processor.h>
#include <ugcs/vsm/posix_file_handle.h>

#include <cstring>
#include <termios.h>
#include <iostream>

using namespace ugcs::vsm;

namespace {

/** Serial port mode on Linux. */
class Linux_serial_mode: public Serial_processor::Stream::Mode {

public:

    /** Constructor. */
    Linux_serial_mode(const Mode& mode);

    /** Setup terminal mode via tcsetattr call. */
    void
    Set_serial_config(int fd, uint8_t min_read);

private:

    /** Maximum possible value for termios VTIME parameter. */
    static constexpr uint8_t MAX_VTIME = 255;

    /** Map provided desired rate value to the closest baud rate constant. */
    tcflag_t
    Map_baud_rate(int baud);

    /** Map provided desired character size value to the corresponding constant. */
    tcflag_t
    Map_char_size(int size);
};

Linux_serial_mode::Linux_serial_mode(const Mode& mode) : Mode(mode)
{
}

void
Linux_serial_mode::Set_serial_config(int fd, uint8_t min_read)
{
    struct termios tio;
    if (tcgetattr(fd, &tio)) {
        VSM_SYS_EXCEPTION("tcgetattr() failed");
    }

    /* Clear all offending output and input flags. */
    tio.c_oflag = 0;
    tio.c_iflag = IGNBRK;
    tio.c_cflag = Map_char_size(char_size) | (stop_bit ? CSTOPB : 0) | CREAD |
            (parity_check ? PARENB : 0) | (parity ? PARODD : 0);
    /* Minimum read size for immediate read completion. */
    tio.c_cc[VMIN] = min_read;
    ssize_t vtime = read_timeout.count() / 100;
    if (vtime > MAX_VTIME) {
        vtime = MAX_VTIME;
    }
    cfsetispeed(&tio, Map_baud_rate(baud));
    cfsetospeed(&tio, Map_baud_rate(baud));
    /* Clear all local flags to disable special processing of data, like
     * canonical mode, echoing, special characters, signals etc. We want clean
     * data stream. */
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = vtime;
    if (tcsetattr(fd, TCSANOW, &tio)) {
        VSM_SYS_EXCEPTION("tcsetattr() failed");
    }
}

tcflag_t
Linux_serial_mode::Map_baud_rate(int baud)
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

tcflag_t
Linux_serial_mode::Map_char_size(int size)
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

/** Extended handle to provide timed read for serial ports. */
class Serial_file_handle: public internal::Posix_file_handle {
public:

    /** Construct based on previously opened descriptor. */
    Serial_file_handle(int fd, const Serial_processor::Stream::Mode& mode);

    /** Configure the descriptor based on current mode. */
    void
    Configure();

private:

    /** Port opening mode. */
    Linux_serial_mode mode;
};

Serial_file_handle::Serial_file_handle(
        int fd,
        const Serial_processor::Stream::Mode& mode):
                Posix_file_handle(fd),
                mode(mode)
{
}

void
Serial_file_handle::Configure()
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        VSM_SYS_EXCEPTION("Failed to get flags");
    }
    flags |= O_NONBLOCK;
    /* FNDELAY should be unset for VMIN/VTIME to work. */
    flags &= ~O_NDELAY;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        VSM_SYS_EXCEPTION("Failed to set flags");
    }

    if (tcflush(fd, TCIFLUSH)) {
        VSM_SYS_EXCEPTION("tcflush() failed");
    }

    /* Set max possible VMIN. During actual reading, the lesser of VMIN and
     * requested size takes precedence. It is impossible to set VMIN greater
     * then MAX_VMIN anyway.
     * Unfortunately the above is not tru in case of OSX.
     * Therefore we set MAX_VMIN to 1 on Mac.
     */
    mode.Set_serial_config(fd, Serial_processor::MAX_VMIN);
}

} /* anonymous namespace */

Serial_processor::Stream::Native_handle::Unique_ptr
Serial_processor::Open_native_handle(const std::string &port_name, const Stream::Mode &mode)
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

    auto handle = std::make_unique<Serial_file_handle>(fd, mode);
    handle->Configure();

    return std::move(handle);
}
