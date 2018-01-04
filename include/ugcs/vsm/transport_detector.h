// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file transport_detector.h
 */

#ifndef _TRANSPORT_DETECTOR_H_
#define _TRANSPORT_DETECTOR_H_

#include <ugcs/vsm/request_worker.h>
#include <ugcs/vsm/socket_processor.h>
#include <ugcs/vsm/properties.h>
#include <ugcs/vsm/regex.h>
#include <ugcs/vsm/shared_mutex_file.h>
#include <set>
#include <unordered_set>

namespace ugcs {
namespace vsm {

/** Manages the detection of connected vehicles based on heuristic protocol
 * analysis.
 * XXX design is broken, total rework is needed.
 */
class Transport_detector : public Request_processor
{
    DEFINE_COMMON_CLASS(Transport_detector, Request_container);
public:
    /** Detector function.
     * User must supply this function via Add_detector() call.
     * It should follow the signature:
     * void Handler(string port, int baud_rate, Socket_address::Ptr, stream)
     * The Handler will get invoked when new port is detected on system for each baud_rate
     * Handler will get invoked once per second for all baudrates until
     * user calls Protocol_detected() with detected==true.
     * After protocol is detected successfully, the framework still checks for stream closure.
     * Once the stream is closed the detection loop for that port gets resumed.
     */
    typedef Callback_proxy<void, std::string, int, Socket_address::Ptr, Io_stream::Ref> Connect_handler;

    /** Builder for connect handler. */
    DEFINE_CALLBACK_BUILDER(
            Make_connect_handler,
            (std::string, int, Socket_address::Ptr, Io_stream::Ref),
            ("", 0, nullptr, nullptr))

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
     * @param handler Callback with signature void Handler(string port, int baud_rate, stream)
     * @param context Context in which the handler will execute
     * @param prefix prefix
     * @param properties Properties instance to get the config from. If not given then
     *                   it is read global from Properties::Get_instance()
     * @param tokenizer  character used as tokenizer in parameters file
     *
     *
     * The function recognizes the following patterns from config file:
     *
     * # for serial connections:
     * connection.serial.use_arbiter = yes|no (default=yes)
     * connection.serial.exclude.\<exclude_id\> = \<regexp\>
     * connection.serial.\<conn_id\>.name = \<regexp\>
     * connection.serial.\<conn_id\>.baud[.baud_id] = \<integer\>
     *
     * # for outgoing tcp connections:
     * connection.tcp_out.\<conn_id\>.address = \<ip address\>|\<dns name\>
     * connection.tcp_out.\<conn_id\>.port = \<1..65535\>|\<common tcp port name\>
     *
     * # for incoming tcp connections:
     * connection.tcp_in.\<conn_id\>.local_address = \<ip address\>
     * connection.tcp_in.\<conn_id\>.local_port = \<1..65535\>|\<common tcp port name\>
     *
     * # for incoming udp connections (will accept packets from any peer):
     * # in this case Write function cannot be used on the returned stream. Only Write_to().
     * connection.udp_any.\<conn_id\>.local_port = \<1..65535\>|\<common udp port name\>
     * connection.udp_any.\<conn_id\>.local_address = \<ip address\>
     *
     * # for incoming udp connections (each remote peer will be treated as separate connection):
     * connection.udp_in.\<conn_id\>.local_port = \<1..65535\>|\<common udp port name\>
     * connection.udp_in.\<conn_id\>.local_address = \<ip address\>
     *
     * # for outgoing udp connections:
     * connection.udp_out.\<conn_id\>.port = \<1..65535\>|\<common udp port name\>
     * connection.udp_out.\<conn_id\>.address = \<ip address\>|\<dns name\>
     * connection.udp_out.\<conn_id\>.local_port = \<1..65535\>|\<common udp port name\>
     * connection.udp_out.\<conn_id\>.local_address = \<ip address\>
     *
     * # for outgoing tcp proxy connections:
     * connection.proxy.\<conn_id\>.address = \<ip address\>|\<dns name\>
     * connection.proxy.\<conn_id\>.port = \<1..65535\>|\<common tcp port name\>
     *
     * # for CAN bus connections:
     * connection.can.\<conn_id\>.name = \<can interface name\>
     *
     *
     * Example ardupilot config file for linux:
     *
     * # Exclude native serial ports from detection of ArduPilot vehicles.
     * connection.serial.exclude.1 = /dev/ttyS.*
     *
     * # Direct USB link to APM appears as port /dev/ttyACM0, with baud rate 115k
     * connection.serial.1.name = /dev/ttyACM0
     * connection.serial.1.baud = 115200
     *
     * # Radio link to APM via 3DRadio appears as generic USB port.
     * connection.serial.2.name = /dev/ttyUSB.*
     * connection.serial.2.baud.1 = 57600
     * connection.serial.2.baud.2 = 34800
     */
    void
    Add_detector(
            Connect_handler handler,
            Request_processor::Ptr context,
            const std::string& prefix = std::string("connection"),
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

    /** Enable/disable port polling */
    void
    Activate(bool activate);

private:
    class Detector_entry : public std::tuple<int, Connect_handler, Request_processor::Ptr>
    {
    public:
        Detector_entry(int b, Connect_handler h, Request_processor::Ptr c):
            std::tuple<int, Connect_handler, Request_processor::Ptr>(b, h, c)
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
        typedef enum {
            NONE,
            CONNECTING,
            CONNECTED
        } State;

        typedef enum {
            SERIAL,
            TCP_IN,
            TCP_OUT,
            UDP_IN,
            UDP_IN_ANY,
            UDP_OUT,
            PROXY,  // outgoing TCP connection to a vehicle proxy.
            CAN     // can bus
        } Type;

        // construtor for serial port
        Port(const std::string &serial_port_name, Type, Request_worker::Ptr w = nullptr);

        // construtor for incoming ip transports
        Port(
            Socket_address::Ptr local_addr,
            Socket_address::Ptr peer_addr,
            Type,
            Request_worker::Ptr worker,
            int timeout = 0);

        ~Port();

        Type
        Get_type() {return type;}

        void
        On_timer();

        void
        Add_detector(int, Connect_handler, Request_processor::Ptr);

        void
        Create_arbiter(Shared_mutex_file::Acquire_handler handler,  Request_worker::Ptr w);

        void
        Reopen_and_call_next_handler();

        void
        Protocol_not_detected(Io_stream::Ref str);

        void
        Open_serial(bool ok_to_open);

        void
        Ip_connected(
                Socket_stream::Ref str,
                Io_result res);

        void
        Proxy_connected(
                Socket_stream::Ref str,
                Io_result res);

        void
        Listener_ready(
                Socket_stream::Ref str,
                Io_result res);

        void
        On_proxy_data_received(
                ugcs::vsm::Io_buffer::Ptr buf,
                ugcs::vsm::Io_result result,
                Socket_stream::Ref str);

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
        Io_stream::Ref stream = nullptr;

        /** Currently connected and handed off proxy/incoming streams. */
        std::list<Io_stream::Ref> sub_streams;

        /** Ongoing socket connecting (including udp) operation. */
        Operation_waiter socket_connecting_op;

        /** Ongoing proxy socket reading operation. */
        Operation_waiter proxy_stream_reader_op;

        /** Listener for TCP_IN and UDP_IN */
        Socket_stream::Ref listener_stream;

        /** Regular expression for this entry. */
        regex::regex re;

        /** Context to execute Tcp_connected() */
        Request_worker::Ptr worker;

        /** serial or tcp... */
        Type type;

        Shared_mutex_file::Ptr arbiter;

        Shared_mutex_file::Acquire_handler arbiter_callback;

        /** retry timeout for failed outgoing connections*/
        std::chrono::seconds retry_timeout;

        /** retry timeout for failed outgoing connections*/
        std::chrono::time_point<std::chrono::steady_clock> last_reopen;
    };

    /** Watchdog timer. */
    bool
    On_timer();

    void
    On_serial_acquired(Io_result r, const std::string& name);

    static std::vector<uint8_t> PROXY_SIGNATURE;
    static constexpr uint8_t PROXY_COMMAND_HELLO = 0;
    static constexpr uint8_t PROXY_COMMAND_WAIT = 1;
    static constexpr uint8_t PROXY_COMMAND_READY = 2;
    static constexpr uint8_t PROXY_COMMAND_NOTREADY = 3;
    static constexpr uint8_t PROXY_RESPONSE_LEN = 5;

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
            int retry_timeout = 1);

    void
    Add_ip_detector_impl(
            Socket_address::Ptr local_addr,
            Socket_address::Ptr remote_addr,
            Port::Type type,
            Connect_handler,
            Request_processor::Ptr,
            Request::Ptr,
            int retry_timeout);

    void
    Add_can_detector(
            const std::string can_interface,
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

    /** Proxy connection timeout. Close connection if no wait packet received from proxy during this time. */
    constexpr static std::chrono::seconds PROXY_TIMEOUT = std::chrono::seconds(4);

    /** Watchdog timer for detection. */
    Timer_processor::Timer::Ptr watchdog_timer;

    Request_worker::Ptr worker;

    bool use_serial_arbiter = true;

    bool detector_active = true;

    static std::string SERIAL_PORT_ARBITER_NAME_PREFIX;

    /** Maximum length of arbiter name. Should be > SERIAL_PORT_ARBITER_NAME_PREFIX.size() */
    constexpr static size_t ARBITER_NAME_MAX_LEN = 100;

    /** Default retry timeout for outgoing TCP connections */
    constexpr static size_t DEFAULT_RETRY_TIMEOUT = 10;

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
} /* namespace ugcs */

#endif /* _TRANSPORT_DETECTOR_H_ */
