/*
// Copyright (c) 2015, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.
 * ssdp_processor.h
 *
 *  Created on: May 5, 2015
 *      Author: j
 */

#ifndef SRC_SSDP_PROCESSOR_H_
#define SRC_SSDP_PROCESSOR_H_

#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/callback.h>
#include <set>

namespace ugcs {
namespace vsm {

class Service_discovery_processor : public Request_processor
{
    DEFINE_COMMON_CLASS(Service_discovery_processor, Request_container);
public:
    Service_discovery_processor(
            Socket_address::Ptr muticast_adress =
                    Socket_address::Create(DEAFULT_DISCOVERY_ADDRESS, DEAFULT_DISCOVERY_PORT)
    );
    virtual ~Service_discovery_processor();

    /** Detector function.
     * User must supply this function via Subscribe_for_service() call.
     * It should follow the signature:
     *
     * void Handler(string type, string name, string location, bool active)
     *
     * parameter active - true if message is ssdp::alive.
     *                    false if message is ssdp::byebye.
     *
     * The Handler will get invoked when NOTIFY or M-SEARCH response is received for given service type.
     */
    typedef Callback_proxy<void, std::string, std::string, std::string, bool> Detection_handler;

    /** Builder for connect handler. */
    DEFINE_CALLBACK_BUILDER (
            Make_detection_handler,
            (std::string, std::string, std::string, bool),
            ("", "", "", true))

    /** Sends out the NOTIFY ssdp:alive on all available interfaces.
     * Responds to M-SEARCH requests for given type.
     *
     * @param location Can contain special tag @local_address@,
     *                 which will be replaced by the address of interface when sending
     *                 out responses and notifications. Thus user application does not
     *                 need to know local ip addresses even if dns is not used.
     *                 Example:
     *                 "http://@local_address@:12345/my_service/location.point"
     *                 The broadcasted packet will have 'Location:' header like this:
     *                 "Location: http://192.168.1.112:12345/my_service/location.point"
     */
    void
    Advertise_service(const std::string& type, const std::string& name, const std::string& location);

    /** Sends out the NOTIFY ssdp:byebye on all available interfaces.
     * Stop responding to M-SEARCH requests for given type.
     */
    void
    Unadvertise_service(const std::string& type, const std::string& name, const std::string& location);

    /** Sends out the M-SEARCH on all available interfaces.
     * Calls handler on each NOTIFY ssdp:alive and M-SEARCH response
     * for given service type.
     * handler must be valid callback until Unsubscribe_from_service called.
     */
    void
    Subscribe_for_service(
            const std::string &type,
            Detection_handler handler,
            Request_processor::Ptr context);

    /** Sends out the M-SEARCH on all available interfaces.
     * In order to receive responses Subscribe_for_service must be called beforehand.
     */
    void
    Search_for_service(const std::string& type);

    /** Remove service type from subscription list.
     * Detection handlers will not be called for this service type.
     */
    void
    Unsubscribe_from_service(const std::string& type);

    /** Get global or create new object instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }
private:

    static std::string DEAFULT_DISCOVERY_ADDRESS;
    static std::string DEAFULT_DISCOVERY_PORT;
    static std::string LOCAL_ADDRESS_IDENTIFIER;

    /** Singleton object. */
    static Singleton<Service_discovery_processor> singleton;

    typedef std::tuple<std::string, std::string, std::string> Service_info;

    typedef std::pair<Operation_waiter, Socket_processor::Stream::Ref> My_socket;

    Socket_address::Ptr multicast_adress;

    Request_worker::Ptr worker;

    My_socket receiver;

    std::map<std::string, My_socket> sender_sockets;

    std::set<Service_info> my_services;

    std::map<std::string, std::pair<Detection_handler, Request_processor::Ptr>> subscribed_services;

    // look for new interfaces each 5 seconds.
    Timer_processor::Timer::Ptr my_timer;

    void
    On_read(
            Io_buffer::Ptr buffer,
            Io_result result,
            Socket_address::Ptr addr,
            std::string stream_id);

    bool
    On_timer();

    void
    On_sender_bound(Socket_processor::Stream::Ref l, Io_result);

    void
    Send_notify(
            Socket_processor::Stream::Ref s,
            const std::string& type,
            const std::string& name,
            const std::string& location);

    void
    Send_response(
            Socket_address::Ptr addr,
            const std::string& type,
            const std::string& name,
            const std::string& location);

    void
    Send_msearch(Socket_processor::Stream::Ref s, const std::string& type);

    /** Starts internal loop and invokes handlers.
     */
    virtual void
    On_enable() override;

    // return true  if already activated
    //        false if was not activated
    bool
    Activate();

    void
    Deactivate();

    void
    On_advertise(
            const std::string type,
            const std::string name,
            const std::string location,
            Request::Ptr request);

    void
    On_unadvertise(
            const std::string type,
            const std::string name,
            const std::string location,
            Request::Ptr request);

    void
    On_subscribe_for_service(
            const std::string type,
            Detection_handler handler,
            Request_processor::Ptr context,
            Request::Ptr request);

    void
    On_unsubscribe_from_service(
            const std::string type,
            Request::Ptr request);

    void
    On_search_for_service(
            const std::string type,
            Request::Ptr request = nullptr);

    void
    On_disable_impl(Request::Ptr request);

    /** Stops internal loop.
     */
    virtual void
    On_disable() override;

    void
    Schedule_read(std::string id);

    static bool
    Has_location_string(const std::string& loc);

    static std::string
    Build_location_string(const std::string& loc, Socket_address::Ptr local_addr);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* SRC_SSDP_PROCESSOR_H_ */
