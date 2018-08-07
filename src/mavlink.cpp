// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * MAVLink protocol messages implementation.
 */

#include <ugcs/vsm/mavlink.h>
#include <ugcs/vsm/debug.h>

#include <sstream>

using namespace ugcs::vsm;
using namespace ugcs::vsm::mavlink;

Io_buffer::Ptr
Payload_base::Get_buffer() const
{
    return Io_buffer::Create(Get_data(), Get_size_v2());
}

const Extension Extension::instance;

namespace {

const char *
Get_type_name(Field_type_id id)
{
    const char *names[] = {
        "NONE",
        "char",
        "int8_t",
        "uint8_t",
        "int16_t",
        "uint16_t",
        "int32_t",
        "uint32_t",
        "int64_t",
        "uint64_t",
        "float",
        "double",
        "uint8_t_mavlink_version"
    };
    static_assert(sizeof(names) / sizeof(names[0]) == FIELD_TYPE_MAX,
                  "Names array size mismatch");
    return names[id];
}

/** Dump value into provided string.
 *
 * @param ss Output stream.
 * @param ptr Pointer to the value storage.
 * @param num Number of values.
 */
template <typename T>
void
Dump_value(std::stringstream &ss, const void *ptr, size_t num)
{
    const T *values = reinterpret_cast<const T *>(ptr);
    if (num == 1) {
        ss << Le(*values);
    } else {
        ss << '{';
        while (num) {
            ss << Le(*values);
            values++;
            num--;
            if (num) {
                ss << ", ";
            }
        }
        ss << '}';
    }
}

/** Specialization for strings. */
template <>
void
Dump_value<char>(std::stringstream &ss, const void *ptr, size_t num)
{
    const char *str = reinterpret_cast<const char *>(ptr);
    ss << '\'';
    while (num) {
        if (!*str) {
            break;
        }
        ss << *str;
        num--;
        str++;
    }
    ss << '\'';
}

template <>
void
Dump_value<int8_t>(std::stringstream &ss, const void *ptr, size_t num)
{
    const int8_t *values = reinterpret_cast<const int8_t *>(ptr);
    if (num == 1) {
        ss << static_cast<int>(*values);
    } else {
        ss << '{';
        while (num) {
            ss << static_cast<int>(*values);
            values++;
            num--;
            if (num) {
                ss << ", ";
            }
        }
        ss << '}';
    }
}

template <>
void
Dump_value<uint8_t>(std::stringstream &ss, const void *ptr, size_t num)
{
    const uint8_t *values = reinterpret_cast<const uint8_t *>(ptr);
    if (num == 1) {
        ss << static_cast<int>(*values);
    } else {
        ss << '{';
        while (num) {
            ss << static_cast<int>(*values);
            values++;
            num--;
            if (num) {
                ss << ", ";
            }
        }
        ss << '}';
    }
}

} /* anonymous namespace */

std::string
Payload_base::Dump() const
{
    std::stringstream ss;
    internal::Field_descriptor *desc = Get_fields();
    const uint8_t *field = reinterpret_cast<const uint8_t *>(Get_data());

    if (Get_size_v1() != Get_size_v2()) {
        ss << "Message " << Get_name() << " (" << Get_size_v1() << "-" << Get_size_v2() << " bytes)\n";
    } else {
        ss << "Message " << Get_name() << " (" << Get_size_v1() << " bytes)\n";
    }

    while (desc->type_id != NONE) {
        ss << Get_type_name(desc->type_id) << ' ';
        ss << desc->name;
        if (desc->array_size > 1) {
            ss << '[' << desc->array_size << ']';
        }
        ss << ": ";
        switch (desc->type_id) {
        case CHAR:
            Dump_value<char>(ss, field, desc->array_size);
            field += sizeof(char) * desc->array_size;
            break;
        case INT8:
            Dump_value<int8_t>(ss, field, desc->array_size);
            field += sizeof(int8_t) * desc->array_size;
            break;
        case UINT8:
            Dump_value<uint8_t>(ss, field, desc->array_size);
            field += sizeof(uint8_t) * desc->array_size;
            break;
        case INT16:
            Dump_value<int16_t>(ss, field, desc->array_size);
            field += sizeof(int16_t) * desc->array_size;
            break;
        case UINT16:
            Dump_value<uint16_t>(ss, field, desc->array_size);
            field += sizeof(uint16_t) * desc->array_size;
            break;
        case INT32:
            Dump_value<int32_t>(ss, field, desc->array_size);
            field += sizeof(int32_t) * desc->array_size;
            break;
        case UINT32:
            Dump_value<uint32_t>(ss, field, desc->array_size);
            field += sizeof(uint32_t) * desc->array_size;
            break;
        case INT64:
            Dump_value<int64_t>(ss, field, desc->array_size);
            field += sizeof(int64_t) * desc->array_size;
            break;
        case UINT64:
            Dump_value<uint64_t>(ss, field, desc->array_size);
            field += sizeof(uint64_t) * desc->array_size;
            break;
        case FLOAT:
            Dump_value<float>(ss, field, desc->array_size);
            field += sizeof(float) * desc->array_size;
            break;
        case DOUBLE:
            Dump_value<double>(ss, field, desc->array_size);
            field += sizeof(double) * desc->array_size;
            break;
        case UINT8_VERSION:
            Dump_value<uint8_t>(ss, field, desc->array_size);
            field += sizeof(uint8_t) * desc->array_size;
            break;
        default:
            ASSERT(false);
        }
        ss << '\n';
        desc++;
    }
    return ss.str();
}

Checksum::Checksum()
{
    Init(accumulator);
}

Checksum::Checksum(const void * buffer, size_t len)
{
    Init(accumulator);
    Calculate(buffer, len, &accumulator);
}

Checksum::Checksum(const Io_buffer::Ptr& buffer)
{
    Init(accumulator);
    Calculate(buffer->Get_data(), buffer->Get_length(), &accumulator);
}

uint16_t
Checksum::Accumulate(const Io_buffer::Ptr& buffer)
{
    Calculate(buffer->Get_data(), buffer->Get_length(), &accumulator);
    return accumulator;
}

uint16_t
Checksum::Accumulate(const void* buffer, size_t len)
{
    Calculate(buffer, len, &accumulator);
    return accumulator;
}

uint16_t
Checksum::Accumulate(uint8_t byte)
{
    return Accumulate(&byte, sizeof(byte));
}

uint16_t
Checksum::Calculate(const void* buffer, size_t len, uint16_t* accumulator)
{
    uint16_t accum;
    const uint8_t* data = static_cast<const uint8_t*>(buffer);

    if (!accumulator) {
        accumulator = &accum;
        Init(accum);
    }

    while (len--) {
        uint8_t tmp;
        tmp = *(data++) ^ (*accumulator & 0xff);
        tmp ^= (tmp << 4);
        *accumulator = (*accumulator >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4);
    }

    return *accumulator;
}

uint16_t
Checksum::Get() const
{
    return accumulator;
}

bool
Checksum::Get_extra_byte_length_pair(
        MESSAGE_ID_TYPE message_id,
        Extra_byte_length_pair& ret,
        const Extension &ext)
{
    const auto *base_map = ext.Get_crc_extra_byte_map();
    auto it = base_map->find(message_id);
    if (it == base_map->end()) {
        return false;
    }
    ret = it->second;
    return true;
}

void
Checksum::Reset()
{
    Init(accumulator);
}

void
Checksum::Init(uint16_t& accumulator)
{
    accumulator = X25_INIT_CRC;
}
