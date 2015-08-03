// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Linux/OSX specific part of socket processor.
 */

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/log.h>

#include <ifaddrs.h>
#include <net/if.h>

std::list<ugcs::vsm::Local_interface>
ugcs::vsm::Socket_processor::Enumerate_local_interfaces()
{
    std::list<ugcs::vsm::Local_interface> ret;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr) {
                auto iface = ret.begin();
                for (; iface != ret.end(); iface++) {
                    if (iface->name == ifa->ifa_name) {
                        break;
                    }
                }
                if (iface == ret.end()) {
                    iface = ret.emplace(ret.end(), ifa->ifa_name, ifa->ifa_flags & IFF_MULTICAST);
                }
                const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(ifa->ifa_addr);
                if (    addr->sin_family == AF_INET
                    &&  addr->sin_addr.s_addr != ntohl(INADDR_LOOPBACK)) {
                    // Do not add loopback to list (OSX reports it as multicast capable, but that is not true.)
                    iface->adresses.emplace_back(ugcs::vsm::Socket_address::Create(*addr));
                }
            }
        }
        freeifaddrs(ifaddr);
    } else {
        LOG("getifaddrs failed %s", ugcs::vsm::Log::Get_system_error().c_str());
    }
    return ret;
}
