// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
#include <ugcs/vsm/peripheral_message.h>

using namespace ugcs::vsm;

Peripheral_message::Peripheral_register::Peripheral_register(uint16_t id,
		PERIPHERAL_TYPE dtype,
		std::string dname,
		std::string dport) {
	device_id = id;
	device_type = dtype;
    device_name = dname.substr(0, 30);
	if(dname.size() > 30)
		device_name.replace(device_name.begin()+26, device_name.begin()+30, "(...)");
	port_name = dport.substr(0, 30);
	if(dport.size() > 30)
		port_name.replace(port_name.begin()+26, port_name.begin()+30, "(...)");
}

Peripheral_message::Peripheral_update::Peripheral_update(uint16_t id,
		PERIPHERAL_STATE state) {
	device_id = id;
	device_state = state;
}
