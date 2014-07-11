// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file hid_processor.h
 *
 * HID devices driver.
 */

#ifndef VSM_DISABLE_HID

#ifndef HID_PROCESSOR_H_
#define HID_PROCESSOR_H_

#include <ugcs/vsm/vsm.h>

namespace ugcs {
namespace vsm {

/** Processor for getting events from raw input device. */
class Hid_processor: public ugcs::vsm::File_processor {
    DEFINE_COMMON_CLASS(Hid_processor, Request_container)
public:
    /** Base class for all Hid_processor exceptions. */
    VSM_DEFINE_EXCEPTION(Exception);
    /** Device not found. Thrown when a specified device not connected. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Not_found_exception);
    /** Device already opened. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Already_opened_exception);
    /** Device is in the state which does not allow request execution. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Invalid_state_exception);

    /** Full device ID. */
    class Device_id {
    public:
        /** Construct device ID from vendor and product IDs.
         *
         * @param vendor_id Vendor ID.
         * @param product_id Product ID.
         */
        Device_id(uint32_t vendor_id, uint32_t product_id):
            id(vendor_id, product_id)
        {}

        /** Get vendor ID of the device. */
        uint32_t
        Get_vendor_id() const
        {
            return std::get<0>(id);
        }

        /** Get product ID of the device. */
        uint32_t
        Get_product_id() const
        {
            return std::get<1>(id);
        }

        /** Get numeric representation of whole the device ID. */
        uint64_t
        Get_numeric() const
        {
            return (static_cast<uint64_t>(std::get<0>(id)) << 32) | std::get<1>(id);
        }

        /** Comparison operator for device IDs. */
        bool
        operator <(const Device_id id) const
        {
            return Get_numeric() < id.Get_numeric();
        }

    private:
        std::tuple<uint32_t, uint32_t> id;
    };

    /** Stream class which represents opened device. */
    class Stream: public File_processor::Stream {
        DEFINE_COMMON_CLASS(Stream, Io_stream)
    public:
        /** Reference type. */
        typedef Reference_guard<Stream::Ptr> Ref;

        /** Interface for platform native device handle. Platform-dependent class
         * should be derived from this one.
         */
        class Native_handle {
        public:
            typedef std::shared_ptr<Native_handle> Ptr;

            /** Human readable device name. */
            std::string name;
            /** Device node file name. */
            std::string file_name;

            /** Create native handle instance. */
            static Ptr
            Create(Device_id device_id);

            virtual
            ~Native_handle()
            {}

            /** Issues SET_REPORT control transfer with the data provided.
             * @throws Invalid_state_exception if device is not connected.
             */
            virtual void
            Set_output_report(Io_buffer::Ptr data, uint8_t report_id = 0) = 0;

            /** Issues GET_REPORT control transfer.
             * @throws Invalid_state_exception if device is not connected.
             */
            virtual Io_buffer::Ptr
            Get_input_report(size_t report_size, uint8_t report_id = 0) = 0;
        };

        /** Construct device stream based on HID processor native handle
         * and File_processor::Stream::Native_handle for read/write operations.
         *
         * @param processor Associated processor instance.
         * @param device_id Target device ID.
         * @param native_handle HID native handle.
         * @param native_file_handle Native file handle for read/write ops.
         */
        Stream(Hid_processor::Ptr processor, Device_id device_id,
                Native_handle::Ptr native_handle,
                File_processor::Stream::Native_handle::Unique_ptr&& native_file_handle);

        /** Get associated device ID. */
        Device_id
        Get_device_id() const
        {
            return device_id;
        }

        /** Issues SET_REPORT control transfer with the data provided.
         * @throws Invalid_state_exception if device is not connected.
         */
        void
        Set_output_report(Io_buffer::Ptr data, uint8_t report_id = 0);

        /** Issues GET_REPORT control transfer.
         * @throws Invalid_state_exception if device is not connected.
         */
        Io_buffer::Ptr
        Get_input_report(size_t report_size, uint8_t report_id = 0);

    private:
        friend class Native_handle;

        /** Associated processor. */
        Hid_processor::Ptr proc;
        /** Associated device ID. */
        Device_id device_id;
        /** Native device handle. */
        Native_handle::Ptr native_handle;
    };

    /** Get global or create new processor instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Construct the processor instance. */
    Hid_processor();

    /** Open device for events reception.
     *
     * @param vendor_id Vendor ID of the device.
     * @param product_id Product ID of the device.
     * @return Created device stream.
     *
     * @throws Not_found_exception if the specified device not found in
     *      connected devices list.
     * @throws Already_opened_exception if the specified device is already
     *      opened in other stream.
     */
    Stream::Ref
    Open(uint32_t vendor_id, uint32_t product_id);

private:
    friend class Stream;

    /** Singleton object. */
    static Singleton<Hid_processor> singleton;

    /** Opened device streams. */
    std::map<Device_id, Stream::Weak_ptr> streams;
    /** Protects streams map. */
    std::mutex stream_mutex;

    /** Platform dependent stream creation method. */
    Stream::Ptr
    Create_stream(Device_id device_id);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* HID_PROCESSOR_H_ */

#endif /* VSM_DISABLE_HID */
