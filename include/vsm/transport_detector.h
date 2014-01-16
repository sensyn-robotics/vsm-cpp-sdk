// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file transport_detector.h
 */

#ifndef TRANSPORT_DETECTOR_H_
#define TRANSPORT_DETECTOR_H_

#include <set>
#include <unordered_set>
#include <vsm/request_worker.h>
#include <vsm/socket_processor.h>
#include <vsm/properties.h>
#include <vsm/regex.h>
#include <vsm/shared_mutex_file.h>

namespace vsm {

/** Manages the detection of connected vehicles based on heuristic protocol
 * analysis.
 */
class Transport_detector : public Request_processor
{
    DEFINE_COMMON_CLASS(Transport_detector, Request_container);
public:

    /** Detector function.
     * User must supply this function via Add_detector() call.
     * It should follow the signature: void Handler(string port, int baud_rate, stream)
     * The Handler will get invoked when new port is detected on system for each baud_rate
     * Handler will get invoked once per second for all baudrates until
     * user calls Protocol_detected() with detected==true.
     * After protocol is detected successfully, the framework still checks for stream closure.
     * Once the stream is closed the detection loop for that port gets resumed.
     */
    typedef Callback_proxy<void, std::string, int, Io_stream::Ref> Connect_handler;

    /** Builder for connect handler. */
    DEFINE_CALLBACK_BUILDER (
            Make_connect_handler,
            (std::string, int, Io_stream::Ref),
            ("", 0, nullptr))

    /** Get global or create new object instance. */
    template <typename... Args>
    static Ptr
    Get_instance(Args &&... args)
    {
        return singleton.Get_instance(std::forward<Args>(args)...);
    }

    /** Add port to ports list with a protocol detector
     * Parses the serial port detection part of given \<properties\> instance.
     *
     * @param prefix  String used as prefix for serial port config in properties file
     * @param handler Callback with signature void Handler(string port, int baud_rate, stream)
     * @param context Context in which the handler will execute
     * @param properties Properties instance to get the config from. If not given then
     *                   it is read global from Properties::Get_instance()
     * @param tokenizer  character used as tokenizer in parameters file
     *
     *
     * The function recognizes the following patterns from config file:
     *
     * # for serial connections:
     * \<prefix\>.use_serial_arbiter = yes|no (default=yes)
     * \<prefix\>.exclude.\<exclude_id\> = \<regexp\>
     * \<prefix\>.\<port_id\>.name = \<regexp\>
     * \<prefix\>.\<port_id\>.baud[.baud_id] = \<integer\>
     *
     * # for outgoing tcp connections:
     * \<prefix\>.\<port_id\>.address = \<ip address\>|\<dns name\>
     * \<prefix\>.\<port_id\>.tcp_port = \<1..65535\>|\<common tcp port name\>
     *
     * # for incoming udp connections:
     * \<prefix\>.\<port_id\>.udp_local_port = \<1..65535\>|\<common tcp port name\>
     * \<prefix\>.\<port_id\>.udp_local_address = \<ip address\>|\<dns name\>
     * \<prefix\>.\<port_id\>.udp_address = \<ip address\>|\<dns name\>
     * \<prefix\>.\<port_id\>.udp_port = \<1..65535\>|\<common tcp port name\>
     *
     *
     *
     * Example ardupilot config file for linux:
     *
     * # Exclude native serial ports from detection of ArduPilot vehicles.
     * vehicle.apm.serial_port.exclude.1 = /dev/ttyS.*
     *
     * # Direct USB link to APM appears as port /dev/ttyACM0, with baud rate 115k
     * vehicle.apm.serial_port.1.name = /dev/ttyACM0
     * vehicle.apm.serial_port.1.baud = 115200
     *
     * # Radio link to APM via 3DRadio appears as generic USB port.
     * vehicle.apm.serial_port.2.name = /dev/ttyUSB.*
     * vehicle.apm.serial_port.2.baud.1 = 57600
     * vehicle.apm.serial_port.2.baud.2 = 34800
     */
    void
    Add_detector(
            const std::string& prefix,
            Connect_handler handler,
            Request_processor::Ptr context,
            Properties::Ptr properties = nullptr,
            char tokenizer = '.');

    /** Notification function.
     * User must call this to signal about not-detected protocol (on a stream given via Connect_handler callback).
     * Transport_detector will execute next callback until there are no more detectors.
     */
    void
    Protocol_not_detected(Io_stream::Ref stream);

    // ======== User interface ends here ========

    Transport_detector();

private:
    class Detector_entry : public std::tuple<int, Connect_handler, Request_processor::Ptr>
    {
    public:
        Detector_entry(int b, Connect_handler h, Request_processor::Ptr c):
            std::tuple<int, Connect_handler, Request_processor::Ptr>(b,h,c)
        {};

        int
        Get_baud()
        {
            return std::get<0>(*this);
        };

        Connect_handler
        Get_handler()
        {
            return std::get<1>(*this);
        };

        Request_processor::Ptr
        Get_ctx()
        {
            return std::get<2>(*this);
        };
    };

public:

    /** Communication port. */
    class Port {
    public:
        typedef enum{
            NONE,
            CONNECTING,
            CONNECTED
        } State;

        typedef enum{
            SERIAL,
            TCP_OUT,
            UDP_IN
        } Type;

        // construtor for serial port
        Port(const std::string &serial_port_name);

        // construtor for incoming ip transports
        Port(Socket_address::Ptr local_addr, Socket_address::Ptr peer_addr, Type, Request_worker::Ptr worker);

        ~Port();

        Io_stream::Ref
        Get_stream();

        Type
        Get_type(){return type;};

        void
        On_timer();

        void
        Add_detector(int, Connect_handler, Request_processor::Ptr);

        void
        Create_arbiter(Shared_mutex_file::Acquire_handler handler,  Request_worker::Ptr w);

        void
        Reopen_and_call_next_handler();

        void
        Open_serial(bool ok_to_open);

        void
        Ip_connected(
                Socket_stream::Ref str,
                Io_result res);

        typedef std::list<Detector_entry> Detector_list;
        /** preconfigured detectors for each baud. */
        Detector_list detectors;

        /** true if given name matches this port's name which can be a regular expression*/
        bool
        Match_name(std::string & name);

    private:

        /** current state */
        State state;

        /** Port name */
        std::string name;

        /** local socket address for listener ports **/
        Socket_address::Ptr local_addr = nullptr;

        /** remote socket address for connected ports **/
        Socket_address::Ptr peer_addr = nullptr;

        /** Current detector. */
        Detector_list::iterator  current_detector;

        /** Opened stream if the port available. */
        Io_stream::Ref stream;

        /** Ongoing socket connecting (including udp) operation. */
        Operation_waiter socket_connecting_op;

        /** Regular expression for this entry. */
        regex::regex re;

        /** Context to execute Tcp_connected() */
        Request_worker::Ptr worker;

        /** serial or tcp... */
        Type type;

        Shared_mutex_file::Ptr arbiter;

        Shared_mutex_file::Acquire_handler arbiter_callback;
    };

    /** Watchdog timer. */
    bool
    On_timer();

    void
    On_serial_acquired(Io_result r, const std::string& name);

private:

    void
    Add_serial_detector(
            const std::string port_regexp,
            int,
            Connect_handler,
            Request_processor::Ptr,
            Request::Ptr);

    void
    Add_ip_detector(
            Socket_address::Ptr local_addr,
            Socket_address::Ptr remote_addr,
            Port::Type type,
            Connect_handler,
            Request_processor::Ptr,
            Request::Ptr);

    void
    Add_blacklisted_impl(Connect_handler handler, const std::string port_regexp,
            Request::Ptr request);

    void
    Protocol_not_detected_impl(Io_stream::Ref stream, Request::Ptr request);

    /** Singleton object. */
    static Singleton<Transport_detector> singleton;

    /** port parameters as read form config file.
     * port names are regular expressions here */
    std::unordered_map<std::string, Port> serial_detector_config;

    /** currently connected ports.
    * port names are real existing system ports here. Includes current port state*/
    std::unordered_map<std::string, Port> active_config;

    /** list of excluded ports per protocol.
     * each protocol maps to a list of regexps defining ports not to be used to detect this protocol*/
    std::unordered_map<Connect_handler, std::list<regex::regex>, Connect_handler::Hasher> port_black_list;

    bool
    Port_blacklisted(const std::string& port, Connect_handler handler);

    /** Watchdog interval. */
    const std::chrono::seconds WATCHDOG_INTERVAL = std::chrono::seconds(1);

    /** TCP connect timeout. */
    constexpr static std::chrono::seconds TCP_CONNECT_TIMEOUT = std::chrono::seconds(10);

    /** Watchdog timer for detection. */
    Timer_processor::Timer::Ptr watchdog_timer;

    Request_worker::Ptr worker;

    bool use_serial_arbiter = true;

    static std::string SERIAL_PORT_ARBITER_NAME_PREFIX;

    /** Maximum length of arbiter name. Should be > SERIAL_PORT_ARBITER_NAME_PREFIX.size() */
    constexpr static size_t ARBITER_NAME_MAX_LEN = 100;

    /** Starts internal port detection loop and invokes supplied handlers each second.
     */
    virtual void
    On_enable() override;
    /** Stops internal port detection loop and invokes supplied handlers each second.
     */
    virtual void
    On_disable() override;

    /** Process disable in the processor context. */
    void
    Process_on_disable(Request::Ptr);
};

} /* namespace vsm */

#endif /* TRANSPORT_DETECTOR_H_ */
