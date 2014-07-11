// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file serial_processor.h
 *
 * Serial port I/O processor.
 */

#ifndef SERIAL_PROCESSOR_H_
#define SERIAL_PROCESSOR_H_

#include <ugcs/vsm/file_processor.h>
#include <ugcs/vsm/log.h>

namespace ugcs {
namespace vsm {

/** Serial ports I/O processor. */
class Serial_processor: public File_processor {
    DEFINE_COMMON_CLASS(Serial_processor, Request_container)
public:

    /** Stream which represents opened serial port. */
    class Stream: public File_processor::Stream {
        DEFINE_COMMON_CLASS(Stream, Io_stream)
    public:
        /** Reference type. */
        typedef Reference_guard<Ptr> Ref;

        /** Communication mode parameters for a serial port. */
        class Mode {
        public:
            Mode() {}

            /** Set baud rate.
             *
             * @param baud Baud rate. The value is rounded to the closest
             *      allowed one.
             */
            Mode &
            Baud(int baud)
            {
                this->baud = baud;
                return *this;
            }

            /** Set character size.
             *
             * @param size Character size in bits.
             */
            Mode &
            Char_size(int size)
            {
                char_size = size;
                return *this;
            }

            /** Set number of stop bits.
             *
             * @param enable Two stop bits if "true", one stop bit otherwise.
             */
            Mode &
            Stop_bit(bool enable)
            {
                stop_bit = enable;
                return *this;
            }

            /** Set parity check/generation mode.
             *
             * @param enable Enable parity check on input and generation on
             *      output when "true".
             */
            Mode &
            Parity_check(bool enable)
            {
                parity_check = enable;
                return *this;
            }

            /** Set parity mode.
             *
             * @param is_odd If "true", then parity for input and output is odd;
             *      otherwise even parity is used.
             */
            Mode &
            Parity(bool is_odd)
            {
                parity = is_odd;
                return *this;
            }

            /** Set read timeout. It specifies the minimum amount of time to
             * wait for data, if maximum number of requested bytes is not
             * received. Note that maximum supported value is platform specific.
             *
             * @param timeout Read timeout.
             * @throw Invalid_param_exception if negative timeout is specified.
             */
            Mode &
            Read_timeout(std::chrono::milliseconds timeout)
            {
                if (timeout.count() < 0) {
                    VSM_EXCEPTION(Invalid_param_exception, "Negative timeout specified.");
                }
                read_timeout = timeout;
                return *this;
            }

        protected:

            /** Serial port operating parameters. */
            // @{
            int baud = 1800;
            int char_size = 8;
            bool stop_bit = false;
            bool parity_check = false;
            bool parity = false;
            std::chrono::milliseconds read_timeout = std::chrono::milliseconds(100);
            // @}
        };

        /** Construct serial processor stream.
         * @param processor Associated processor.
         * @param port_name Serial port name.
         * @param mode Mode for the port opening.
         * @param native_handle Native handle to be used for stream operations.
         */
        Stream(Serial_processor::Ptr processor,
                const std::string& port_name,
                const Stream::Mode &mode,
                Native_handle::Unique_ptr&& native_handle);

    private:
        /** Associated processor. */
        Serial_processor::Ptr processor;
        /** Port communication mode settings. */
        Mode mode;
    };

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    Serial_processor();

    /** Open serial port for communication.
     *
     * @param port_name Platform-specific port name. Since on most OS'es serial
     *      ports can have arbitrary names, defined by vendor, user should
     *      specify necessary name. On Unix platform it is path to device,
     *      usually "/dev/ttyS<n>", on Windows it is typically names "COM<n>".
     * @param mode Parameters for the port set up.
     * @return Opened stream for the port.
     *
     * @throws Invalid_param_exception if the specified mode contains invalid
     *      settings.
     * @throws Not_found_exception if the specified path does not exist when it
     *      must exist based on provided opening mode.
     * @throws Permission_denied_exception if there is insufficient permissions
     *      for file creation.
     * @throws Exception in case of any other error returned by platform.
     */
    Stream::Ref
    Open(const std::string &port_name, const Stream::Mode &mode = Stream::Mode());

    /** Platform-specific enumeration of available serial ports.
     * This function returns the list of currently valid serial ports.
     * It can change over time when usb cables get connected/disconnected
     *
     * @return list of port names which can be used when calling Open().
     */
    static std::list<std::string>
    Enumerate_port_names();

private:
    /** Singleton object. */
    static Singleton<Serial_processor> singleton;

    /** Open platform dependent native handle for a given port and mode. */
    Stream::Native_handle::Unique_ptr
    Open_native_handle(const std::string &port_name, const Stream::Mode &mode);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* SERIAL_PROCESSOR_H_ */
