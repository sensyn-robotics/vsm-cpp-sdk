// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Windows specific part of socket processor.
 */

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/log.h>
#include <iphlpapi.h>

std::list<ugcs::vsm::Local_interface>
ugcs::vsm::Socket_processor::Enumerate_local_interfaces()
{
    std::list<ugcs::vsm::Local_interface> ret;

    PIP_INTERFACE_INFO i_info = nullptr;
    PMIB_IPADDRTABLE a_info = nullptr;
    ULONG buf_len = sizeof(IP_INTERFACE_INFO);
    DWORD result = 0;

    do {
        if (i_info) {
            LocalFree(i_info);
        }
        i_info = reinterpret_cast<PIP_INTERFACE_INFO>(LocalAlloc(LPTR, buf_len));
        if (i_info) {
            result = GetInterfaceInfo(i_info, &buf_len);
        } else {
            goto error_exit;
        }
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result != NO_ERROR) {
        goto error_exit;
    }

    buf_len = sizeof(MIB_IPADDRTABLE);
    do {
        if (a_info) {
            LocalFree(a_info);
        }
        a_info = reinterpret_cast<PMIB_IPADDRTABLE>(LocalAlloc(LPTR, buf_len));
        if (a_info) {
            result = GetIpAddrTable(a_info, &buf_len, 0);
        } else {
            goto error_exit;
        }
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result != NO_ERROR) {
        goto error_exit;
    }

    for (int ifnum = 0; ifnum < i_info->NumAdapters; ifnum++) {
        // Convert interface name to utf8.
        unsigned  MAX_LEN = 100;
        char ifname[MAX_LEN + 1];
        const WCHAR* istr = i_info->Adapter[ifnum].Name;
        std::mbstate_t mbstate = std::mbstate_t();
        std::wcsrtombs(ifname, &istr, MAX_LEN, &mbstate);
        ifname[MAX_LEN] = 0;
        auto iface = ret.begin();
        for (; iface != ret.end(); iface++) {
            if (iface->name == ifname) {
                break;
            }
        }
        if (iface == ret.end()) {
            // Windows does not have special flag for multicast capable interface.
            iface = ret.emplace(ret.end(), std::string(ifname));
            iface->is_multicast = true;
            // There is no loopback interface on windows.
            iface->is_loopback = false;
        }

        unsigned int ifindex = i_info->Adapter[ifnum].Index;
        // If there is a valid address associated with this interface add it to the list.
        for (unsigned int anum = 0; anum < a_info->dwNumEntries; anum++) {
            if (a_info->table[anum].dwIndex == ifindex) {
                sockaddr_in addr;
                addr.sin_addr.S_un.S_addr = a_info->table[anum].dwAddr;
                iface->adresses.emplace_back(ugcs::vsm::Socket_address::Create(addr));
            }
        }
    }
error_exit:
    if (i_info) {
        LocalFree(i_info);
    }
    if (a_info) {
        LocalFree(a_info);
    }
    return ret;
}
