// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file peripheral_message.h
 */
#ifndef _PERIPHERAL_MESSAGE_H_
#define _PERIPHERAL_MESSAGE_H_

#include <ugcs/vsm/reference_guard.h>
#include <set>
#include <ugcs/vsm/io_stream.h>


namespace ugcs {
namespace vsm {
namespace Peripheral_message {


	/** Enum for device types */
	enum class PERIPHERAL_TYPE  {
		PERIPHERAL_DEVICE_UNKNOWN = 0,			// Unknown device
		PERIPHERAL_DEVICE_ADSB = 1				// ADS-B device
	};

	/** Enum for device states */
	enum class PERIPHERAL_STATE {
		PERIPHERAL_STATE_DISCONNECTED = 0,		// Device disconnected
		PERIPHERAL_STATE_HEARTBEAT_OK = 1,		// Device reports heartbeat ok
		PERIPHERAL_STATE_HEARTBEAT_NOK = 2		// Device reports heartbeat missing
	};

	/** Class that should contain report information.
	 * To be given to CUCS on new device detection.
	 * Will be sent to notify server.
	 */
	class Peripheral_register : public std::enable_shared_from_this<Peripheral_register> {
		DEFINE_COMMON_CLASS(Peripheral_register, Peripheral_register);

	public:
		/** Constructor */
		Peripheral_register(uint16_t, PERIPHERAL_TYPE, std::string, std::string);

		/** Returns device ID */
		uint16_t
		Get_id() { return device_id; }

		/** Returns device type */
		PERIPHERAL_TYPE
		Get_dev_type() { return device_type; }

		/** Returns device name */
		std::string
		Get_name() { return device_name; }

		/** Returns device port */
		std::string
		Get_port() { return port_name; }

	private:
	    /** Device identification number */
	    uint16_t device_id;

	    /** Device type */
	    PERIPHERAL_TYPE device_type;

	    /** Name information for device identification */
	    std::string device_name;

	    /** Port information for device identification */
	    std::string port_name;
	};

	class Peripheral_update : public std::enable_shared_from_this<Peripheral_update> {
		DEFINE_COMMON_CLASS(Peripheral_update, Peripheral_update);


	public:
		/** Constructor */
		Peripheral_update(uint16_t, PERIPHERAL_STATE);

		/** Returns device ID */
		uint16_t
		Get_id() { return device_id; }

		/** Returns device state */
		PERIPHERAL_STATE
		Get_state() { return device_state; }

	private:
	    /** Device identification number */
	    uint16_t device_id;

	    /** Device state information */
	    PERIPHERAL_STATE device_state;
	};

} /* namespace Peripheral_message */
} /* namespace vsm */
} /* namespace ugcs */

#endif /* _PERIPHERAL_MESSAGE_H_ */
