// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/peripheral_device.h>
#include <set>

std::set<uint8_t> ugcs::vsm::Peripheral_device::used_id_list;

using namespace ugcs::vsm;

Peripheral_device::Peripheral_device(Peripheral_message::PERIPHERAL_TYPE dev_type)
{
	device_type = dev_type;
	device_id = Get_new_id();
}

uint16_t
Peripheral_device::Get_new_id()
{
	int type_prefix = static_cast<uint8_t>(device_type) << 10;
	return type_prefix + Get_next_free_id();
}

uint16_t
Peripheral_device::Get_next_free_id() {
	uint16_t next_free_id;

	for (next_free_id = 0; next_free_id < 1024; next_free_id++) {
		if(used_id_list.find(next_free_id) == used_id_list.end()) {
			break;
		}
	}
	if (next_free_id > 1023) {
		LOG_ERROR("Out of free device IDs - too many devices connected!");
		ASSERT(false);
	}
	used_id_list.insert(next_free_id);
	return next_free_id;
}

void
Peripheral_device::Remove_id() {
	used_id_list.erase(device_id);
}
