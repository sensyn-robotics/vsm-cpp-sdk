// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink.h
 *
 * MAVLink protocol messages.
 */

#ifndef MAVLINK_H_
#define MAVLINK_H_

#include <ugcs/vsm/endian.h>
#include <ugcs/vsm/io_buffer.h>

#include <cstring>
#include <map>
#include <cmath>

namespace ugcs {
namespace vsm {

/** All MAVLink definitions reside in this namespace. */
namespace mavlink {

enum {
    /** Current protocol version value. */
    VERSION = 3,
    /** Starting byte of Mavlink packet. */
    START_SIGN = 0xfe,
    /** @ref System_id value denoting an unknown system, or all systems
     * depending on context.
     */
    SYSTEM_ID_NONE = 0,
    /** Maximum mavlink packet size on the wire.
     * 6 (header length) + 255 (max payload length) + 2 (checksum)*/
    MAX_MAVLINK_PACKET_SIZE = 263
};

/** ID for field type in MAVLink message. */
enum Field_type_id {
    /** Special value for internal usage. Indicates that value not present. */
    NONE,
    /* MAVLink standard types follow. */
    CHAR,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    UINT8_VERSION,

    /** Number of valid types. */
    FIELD_TYPE_MAX
};

/** Default value for a Mavlink field. */
template<class T, class = void>
struct Field_default_value {
};

/** Partial specialization for integer types. */
template<class T>
struct Field_default_value<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    /** Default value. */
    static constexpr T value = std::numeric_limits<T>::max();

    /** Default value checker. */
    static bool
    Is_default(T val)
    {
        return val == value;
    }
};

/** Partial specialization for floating point types. */
template<class T>
struct Field_default_value<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
    static constexpr T value = std::numeric_limits<T>::quiet_NaN();

    /** Default value checker. */
    static bool
    Is_default(T val)
    {
        return std::isnan(val);
    }
};

/** Field value in MAVLink message.
 * @param T Integer underlying type.
 * @param id ID for the type.
 * @param initial_value Initial value for the field. It's type is fixed to
 *       integer because floating point numbers cannot be template parameters.
 */
template <typename T, Field_type_id id, long initial_value = 0>
class Value {
public:
    /** Construct value.
     *
     * @param value Value in host byte order.
     */
    Value(T value = initial_value): value(value)
    {}

    /** Assign new value.
     *
     * @param value Value in host byte order.
     */
    Value &
    operator =(T value)
    {
        this->value = value;
        return *this;
    }

    /** Cast to underlying type.
     *
     * @return Value in host byte order.
     */
    operator T() const
    {
        return value;
    }

    /** Get the value of underlying type.
     *
     * @return Value in host byte order.
     */
    T
    Get() const
    {
        return value;
    }

    /** Get type ID for this value. */
    constexpr Field_type_id
    Get_type_id()
    {
        return id;
    }

    /** Reset to default value used in UgCS as a "not present" indication. */
    void
    Reset()
    {
        value = Field_default_value<T>::value;
    }

    /** Check, if the field is reset. */
    bool
    Is_reset() const
    {
        return Field_default_value<T>::Is_default(value);
    }

private:
    /** Stored value (in wire byte order). */
    Le_value<T> value;
} __PACKED;

// @{
/** Value containers for all MAVLink types. */
typedef Value<int8_t, CHAR> Char;
typedef Value<int8_t, INT8> Int8;
typedef Value<uint8_t, UINT8> Uint8;
typedef Value<int16_t, INT16> Int16;
typedef Value<uint16_t, UINT16> Uint16;
typedef Value<int32_t, INT32> Int32;
typedef Value<uint32_t, UINT32> Uint32;
typedef Value<int64_t, INT64> Int64;
typedef Value<uint64_t, UINT64> Uint64;
typedef Value<float, FLOAT> Float;
typedef Value<double, DOUBLE> Double;
typedef Value<uint8_t, UINT8_VERSION, VERSION> Uint8_version;
// @}

/** Field in MAVLink message which is array of MAVLink values.
 * @param TValue MAVLink value type. Should be Value<> template instantiation.
 * @param size Array size.
 */
template <class TValue, size_t size>
class Value_array {
private:
    /** Values array itself. */
    TValue data[size];
public:
    /** Access operator. */
    TValue &
    operator [](size_t index)
    {
        if (index >= size) {
            VSM_EXCEPTION(Invalid_param_exception, "Index out of range");
        }
        return data[index];
    }

    /** Reset all array values to UgCS default values. */
    void
    Reset()
    {
        for (auto& elem: data) {
            elem.Reset();
        }
    }
} __PACKED;

/** Partial specialization for characters array so it can be conveniently used
 * with string semantic.
 */
template <size_t size>
class Value_array<Char, size> {
private:
    /** Stored characters array. */
    Char data[size];
public:
    /** Access operator. */
    Char &
    operator [](size_t index)
    {
        if (index >= size) {
            VSM_EXCEPTION(Invalid_param_exception, "Index out of range");
        }
        return data[index];
    }

    /** Get string length. In MAVLink character string may be NULL terminated or
     * may not be if it fully occupies the array.
     *
     * @return String length in bytes.
     */
    size_t
    Get_length() const
    {
        size_t len = 0;
        for (Char c: data) {
            if (!c) {
                return len;
            }
            len++;
        }
        return len;
    }

    /** Get string value for the characters array. */
    std::string
    Get_string() const
    {
        return std::string(reinterpret_cast<const char *>(data), Get_length());
    }

    /** Reset all array values to UgCS default values (zeros for string). */
    void
    Reset()
    {
        for (auto& elem: data) {
            elem = 0;
        }
    }

#ifndef NO_DOXYGEN // Doxygen complains about such operator...
    /** Cast value to string. */
    operator std::string() const
    {
        return Get_string();
    }
#endif

    /** Compare with string. */
    bool
    operator ==(const char *str)
    {
        return Get_string() == str;
    }

    /** Compare with string. */
    bool
    operator !=(const char *str)
    {
        return Get_string() != str;
    }

    /** Assign string to the characters array. In case the string exceeds the
     * array size it is truncated without terminated NULL character (as per
     * MAVLink strings description).
     *
     * @param str String to assign.
     */
    Value_array &
    operator =(const char *str)
    {
        size_t idx;
        for (idx = 0; idx < size && *str; idx++, str++) {
            data[idx] = *str;
        }
        if (idx < size) {
            data[idx] = 0;
        }
        return *this;
    }

    /** Assign string to the characters array. In case the string exceeds the
     * array size it is truncated without terminated NULL character (as per
     * MAVLink strings description).
     *
     * @param str String to assign.
     */
    Value_array &
    operator =(const std::string &str)
    {
        return *this = str.c_str();
    }

} __PACKED;

namespace internal {

/** Descriptor field in a MAVLink message. Used for accessing MAVLink fields
 * dynamically (e.g. when dumping the message into string).
 */
struct Field_descriptor {
    /** Field name. */
    const char *name;
    /** Type ID for this field. NONE for terminating element. */
    Field_type_id type_id;
    /** Array with the specified size if greater than one, single value if one. */
    size_t array_size;
};

} /* namespace internal */

/** A pair of values representing CRC extra byte and length of the Mavlink
 * message payload. */
typedef std::pair<uint8_t, uint16_t> Extra_byte_length_pair;

/** Message id implementation type capable to hold a message id value from
 * any extension (i.e. MESSAGE_ID from any extension can be assigned to this
 * type). */
typedef uint8_t MESSAGE_ID_TYPE;

/* Forward declaration for a type which will be fully defined in generated headers. */
enum MESSAGE_ID: MESSAGE_ID_TYPE;

/** Standard kind of Mavlink protocol. */
struct Mavlink_kind_standard {
    /** System id type. */
    typedef uint8_t System_id;

    /** Wire-type format of Mavlink system id. */
    typedef Uint8 System_id_wire;
};

/** UgCS flavor of Mavlink protocol. */
struct Mavlink_kind_ugcs {
    /** System id type. */
    typedef uint32_t System_id;

    /** Wire-type format of Mavlink system id. */
    typedef Uint32 System_id_wire;
};

/** Common type for Mavlink system id which is able to hold system id values
 * from any kind of Mavlink, currently Mavlink_kind_standard, Mavlink_kind_ugcs.
 */
typedef uint32_t System_id_common;

/** This class defines properties of particular protocol extension. */
class Extension {
public:

    /** Virtual destructor. */
    virtual
    ~Extension() {}

    /** Get the extension instance. */
    static const Extension &
    Get()
    {
        return instance;
    }

    /** Get extension name. */
    virtual std::string
    Get_name() const
    {
        return std::string();
    }

    /** Get map for extra bytes used in CRC calculations;. */
    virtual const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> *
    Get_crc_extra_byte_map() const
    {
        return &crc_extra_bytes_length_map;
    }
private:
    static const Extension instance;
    /** Mapping from Mavlink message id to @ref Extra_byte_length_pair. Filled
     * automatically by generator.
     */
    static const std::map<MESSAGE_ID_TYPE, Extra_byte_length_pair> crc_extra_bytes_length_map;
};

/** Base class for MAVLink message payloads. */
class Payload_base: public std::enable_shared_from_this<Payload_base> {
    DEFINE_COMMON_CLASS(Payload_base, Payload_base)
public:

    virtual
    ~Payload_base()
    {}

    /** Get size of the message payload in bytes. */
    virtual size_t
    Get_size() const = 0;

    /** Get Io_buffer instance which contains current content of the message. */
    Io_buffer::Ptr
    Get_buffer() const;

    /** Dump message content in human-readable format into a string. */
    std::string
    Dump() const;

    /** Get message name. */
    virtual const char *
    Get_name() const = 0;

    /** Get message id. */
    virtual MESSAGE_ID_TYPE
    Get_id() const = 0;

    /** Get extra byte for CRC calculation. */
    virtual uint8_t
    Get_extra_byte() const = 0;

    /** Reset all fields to UgCS default values. */
    virtual void
    Reset() = 0;

protected:
    /** Get raw data of the message. */
    virtual const void *
    Get_data() const = 0;

    /** Get fields description.
     *
     * @return Array of field descriptors. Last element should have "type_id"
     *      value NONE.
     */
    virtual internal::Field_descriptor *
    Get_fields() const = 0;
};

/** Generalized MAVLink message payload.
 * @param TData Structure for payload data.
 */
template <class TData, internal::Field_descriptor *fields, const char *msg_name,
    MESSAGE_ID_TYPE msg_id, uint8_t extra_byte>
class Payload: public Payload_base {
    DEFINE_COMMON_CLASS(Payload, Payload_base)
public:
    /** All fields are initialized to default values. */
    Payload() = default;

    /** Parse message from data buffer. The buffer should contain data on wire
     * (in network byte order). Data size should not be less than expected
     * payload size otherwise Invalid_param_exception is thrown.
     *
     * @param buf Pointer to data buffer.
     * @param size Size of data available.
     * @throws Invalid_param_exception if size is less than expected payload
     *      size;
     */
    Payload(const void *buf, size_t size)
    {
        if (size < sizeof(data)) {
            VSM_EXCEPTION(Invalid_param_exception, "Too small buffer provided");
        }
        memcpy(&data, buf, sizeof(data));
    }

    /**
     * Convenience variation of previous constructor.
     * @param buffer Buffer with data.
     */
    Payload(Io_buffer::Ptr buffer):
        Payload(buffer->Get_data(), buffer->Get_length())
    {}

    /** Get payload size in bytes for this type of message. */
    static constexpr size_t
    Get_payload_size()
    {
        return sizeof(TData);
    }

    /** Get size of the message payload in bytes. */
    virtual size_t
    Get_size() const override
    {
        return Get_payload_size();
    }

    /** Get message name. */
    virtual const char *
    Get_name() const override
    {
        return msg_name;
    }

    /** Get message id. */
    virtual MESSAGE_ID_TYPE
    Get_id() const override
    {
        return msg_id;
    }

    /** Get extra byte for CRC calculation. */
    virtual uint8_t
    Get_extra_byte() const override
    {
        return extra_byte;
    }

    /** Reset all fields to UgCS default values. */
    virtual void
    Reset() override
    {
        data.Reset();
    }

    /** Compare if data in the payload is bit-same with another payload. */
    bool
    operator ==(const Payload &rhs) const
    {
        return !memcmp(&data, &rhs.data, sizeof(data));
    }

    /** Compare if data in the payload is not bit-same with another payload. */
    bool
    operator !=(const Payload &rhs) const
    {
        return memcmp(&data, &rhs.data, sizeof(data));
    }

    /** Pointer access semantics to payload. */
    TData *
    operator ->()
    {
        return &data;
    }

    /** Constant pointer access semantics to payload. */
    const TData *
    operator ->() const
    {
        return &data;
    }

    /** Dereference access semantics to payload. */
    TData &
    operator *()
    {
        return data;
    }

    /** Constant dereference access semantics to payload. */
    const TData &
    operator *() const
    {
        return data;
    }

private:
    /** Message payload which can be directly accessed and modified. */
    TData data;

    /** Get raw data of the message. */
    virtual const void *
    Get_data() const override
    {
        return &data;
    }

    /** Get fields description.
     *
     * @return Array of field descriptors. Last element should have "type_id"
     *      value NONE.
     */
    virtual internal::Field_descriptor *
    Get_fields() const override
    {
        return fields;
    }
};

/** Helper for static (compile time) mapping from Mavlink message ID to
 * corresponding payload type.
 */
template <MESSAGE_ID_TYPE message_id, class Extension_type = Extension>
struct Payload_type_mapper {
    /** Specific payload type. */
    typedef Payload_base type;
};

/** Specific Mavlink message with sender information from the header. */
template<MESSAGE_ID_TYPE message_id, class Extension_type = Extension>
class Message {
    DEFINE_COMMON_CLASS(Message)
public:

    /** Construct message based on Mavlink payload and fixed header important
     * fields.
     * @throws Invalid_param_exception if size of the buffer is less than
     * expected payload size.
     */
    Message(System_id_common system_id, uint8_t component_id, Io_buffer::Ptr buffer) :
        payload(buffer),
        sender_system_id(system_id),
        sender_component_id(component_id) {}

    /** Get system id of the sender. */
    System_id_common
    Get_sender_system_id() const
    {
        return sender_system_id;
    }

    /** Get component id of the sender. */
    uint8_t
    Get_sender_component_id() const
    {
        return sender_component_id;
    }

    /** Payload of the message. */
    typename Payload_type_mapper<message_id, Extension_type>::type payload;

private:

    /** System id of the sending side. */
    System_id_common sender_system_id;

    /** Component id of the sending side. */
    uint8_t sender_component_id;
};

/** Mavlink compatible checksum (ITU X.25/SAE AS-4 hash) calculation class. It
 * can be used statically to calculate checksum of arbitrary buffer, or used as
 * an instance to incrementally accumulate checksum for multiple buffers.
 */
class Checksum {
public:
    /** Base class for exceptions thrown by this class. */
    VSM_DEFINE_EXCEPTION(Exception);
    /** Exception for unknown message ID. */
    VSM_DEFINE_DERIVED_EXCEPTION(Exception, Invalid_id_exception);

    /** Instance initialized with initial checksum value for empty buffer. */
    Checksum();

    /** Instance initialized and checksum is calculated based on provided buffer. */
    Checksum(const void* buffer, size_t len);

    /** Instance initialized and checksum is calculated based on provided buffer. */
    Checksum(const Io_buffer::Ptr& buffer);

    /**
     * Incrementally calculate the checksum of a new buffer and accumulate it
     * to the current value.
     * @param buffer Byte buffer.
     * @param len Size of the buffer.
     * @return New checksum value.
     */
    uint16_t
    Accumulate(const void* buffer, size_t len);

    /**
     * Incrementally calculate the checksum of a new buffer and accumulate it
     * to the current value.
     * @param buffer Byte buffer.
     * @return New checksum value.
     */
    uint16_t
    Accumulate(const Io_buffer::Ptr& buffer);

    /**
     * Incrementally calculate the checksum of one new byte and accumulate it
     * to the current value.
     * @param byte byte.
     * @return New checksum value.
     */
    uint16_t
    Accumulate(uint8_t byte);

    /** Get current checksum value. */
    uint16_t
    Get() const;

    /** Calculate checksum either incrementally or from initial seed value.
     * @param buffer Byte buffer.
     * @param len Size of the buffer.
     * @param accumulator If not @a nullptr, then incremental calculation is
     * made, otherwise initial seed value is used.
     * @return Calculated checksum value.
     */
    static uint16_t
    Calculate(const void* buffer, size_t len, uint16_t* accumulator = nullptr);

    /**
     * Get CRC extra byte and expected payload length of a specific Mavlink
     * message type.
     * @throws Invalid_id_exception for unknown Mavlink messages.
     *
     * @param message_id Mavlink message id.
     * @param ext Extension the message belongs to.
     * @return CRC extra byte and length pair mavlink::Extra_byte_length_pair.
     */
    static Extra_byte_length_pair
    Get_extra_byte_length_pair(MESSAGE_ID_TYPE message_id, const Extension &ext = Extension::Get());

    /** Reset checksum to initial seed value for zero-length buffer. */
    void
    Reset();

private:

    enum {
        /** Initial seed value. */
        X25_INIT_CRC = 0xffff
    };

    /** Initialize accumulator with initial seed value. */
    static void
    Init(uint16_t& accumulator);

    /** Accumulated CRC value. */
    uint16_t accumulator;
};

/** Fixed Mavlink header. */
template<typename Mavlink_kind>
struct Header {
    /** Packet start signature, should have @ref ugcs::vsm::mavlink::START_SIGN value. */
    Uint8 start_sign;
    /** Size of payload which follows this header. */
    Uint8 payload_len;
    /** Sequence number for the message. */
    Uint8 seq;
    /** Sender system ID. */
    typename Mavlink_kind::System_id_wire system_id;
    /** Sender component ID. */
    Uint8 component_id;
    /** Type ID for the payload which follows this header. */
    Uint8 message_id;
} __PACKED;

} /* namespace mavlink */

} /* namespace vsm */
} /* namespace ugcs */

/* Include files generated by mavgen.py. */
#include <ugcs/vsm/auto_mavlink_enums.h>
#include <ugcs/vsm/auto_mavlink_messages.h>

#endif /* MAVLINK_H_ */
