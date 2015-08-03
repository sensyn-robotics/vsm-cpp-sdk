// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file peripheral_device.h
 */
#ifndef _PERIPHERAL_DEVICE_H_
#define _PERIPHERAL_DEVICE_H_

#include <ugcs/vsm/reference_guard.h>
#include <ugcs/vsm/peripheral_message.h>
#include <set>
#include <ugcs/vsm/io_stream.h>


namespace ugcs {
namespace vsm {

/** Base interface for a peripheral device.
 * Provides unique ID and device type enumeration.
 */
class Peripheral_device {

public:

    /** Default constructor */
	Peripheral_device(Peripheral_message::PERIPHERAL_TYPE);

    /**
     * Returns device id
     */
    virtual uint16_t
    Get_id() { return device_id; }

    /**
     * Returns device type
     */
    virtual Peripheral_message::PERIPHERAL_TYPE
    Get_dev_type() { return device_type; }

    /**
     * Returns a friendly device name
     */
    virtual std::string
    Get_friendly_name() { return friendly_name; };

    /**
     * Returns a port name
     */
    virtual std::string
    Get_port_name() { return port_name; }

protected:

    /** Device identification number */
    uint16_t device_id;

    /** Device type */
    Peripheral_message::PERIPHERAL_TYPE device_type;

    /** Name information for device identification */
    std::string friendly_name;

    /** Port information for device identification */
    std::string port_name;

    /** Returns next free ID */
    uint16_t
    Get_new_id();

    /** Returns next free ID */
    static uint16_t
    Get_next_free_id();

    /** Releases ID to be used by other devices */
    void
    Remove_id();

private:
    /** A list of used IDs */
    static std::set<uint8_t> used_id_list;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _PERIPHERAL_DEVICE_H_ */
