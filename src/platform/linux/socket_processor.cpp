// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/* Linux specific part of socket processor.
 */

#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/log.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
//#include <ifaddrs.h>

void
ugcs::vsm::Socket_processor::On_bind_can(
    Io_request::Ptr request,
    std::vector<int> filter_messages,
    Stream::Ptr stream,
    std::string ifname)
{
    sockets::Socket_handle s = INVALID_SOCKET;
    struct sockaddr_can addr;
    struct ifreq ifr;

    ASSERT(stream->Get_type()== Io_stream::Type::CAN);

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s == INVALID_SOCKET) {
        LOG_INFO("Cannot create can socket");
        request->Set_result_arg(Io_result::BAD_ADDRESS);
        request->Complete();
    }

    std::vector<struct can_filter> rfilter;

    for (auto f : filter_messages) {
        rfilter.emplace_back(can_filter{static_cast<canid_t>(f), static_cast<canid_t>(f)});
    }

    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, rfilter.data(), sizeof(struct can_filter) * rfilter.size());

    strcpy(ifr.ifr_name, ifname.c_str());
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (sockets::Make_nonblocking(s) != 0)
    {/* Fatal here. */
        sockets::Close_socket(s);
        VSM_SYS_EXCEPTION("Socket %d failed to set Nonblocking", s);
    }

    if(bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0) {
        auto locker = request->Lock();
        if (request->Is_processing())
        {// request status is still OK
            streams[stream] = stream;
            stream->Set_socket(s);
            stream->Set_state(Io_stream::State::OPENED);
            request->Set_result_arg(Io_result::OK, locker);
        }
        else
        {// cancel or abort requested.
            sockets::Close_socket(s);
            stream->Set_state(Io_stream::State::CLOSED);
            request->Set_result_arg(Io_result::CANCELED, locker);
        }
        request->Complete(Request::Status::OK, std::move(locker));
    } else {
        LOG_INFO("Bind failed: %s", Log::Get_system_error().c_str());
    }
}
