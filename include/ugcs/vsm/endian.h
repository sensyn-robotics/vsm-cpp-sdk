// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file endian.h
 *
 * VSM definitions to work with byte order. There is "endian.h" file present on
 * some systems but it is not standardized and thus not cross-platform.
 */

#ifndef VSM_ENDIAN_H_
#define VSM_ENDIAN_H_

#include <ugcs/vsm/defs.h>

#include <stdint.h>

namespace ugcs {
namespace vsm {

namespace internal {

/** Value for testing system endianness. */
static const uint16_t endianness = 0x0102;

}

/** Check if the system is little-endian. */
constexpr bool
Is_system_le()
{
    return *reinterpret_cast<const uint8_t *>(&internal::endianness) == 0x02;
}

/** Check if the system is big-endian. */
constexpr bool
Is_system_be()
{
    return !Is_system_le();
}

//XXX ifdef GCC
/** Swap bytes in 16-bits integer value. */
#define VSM_BSWAP16(x)      (((x) >> 8) | ((x) << 8))
/** Swap bytes in 32-bits integer value. */
#define VSM_BSWAP32(x)      __builtin_bswap32(x)
/** Swap bytes in 64-bits integer value. */
#define VSM_BSWAP64(x)      __builtin_bswap64(x)

/** Stub for easier conversion functions generalization. */
template <typename T>
constexpr T
Convert_be_8(T x)
{
    return x;
}

/** Convert 16 bits value byte order from BE to host byte order and vice versa.
 * Template parameters:
 * - *T* 16-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_be_16(T x)
{
    return Is_system_be() ? x : VSM_BSWAP16(x);
}

/** Convert 32 bits value byte order from BE to host byte order and vice versa.
 * Template parameters:
 * - *T* 32-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_be_32(T x)
{
    return Is_system_be() ? x : VSM_BSWAP32(x);
}

/** Convert 64 bits value byte order from BE to host byte order and vice versa.
 * Template parameters:
 * - *T* 64-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_be_64(T x)
{
    return Is_system_be() ? x : VSM_BSWAP64(x);
}

/** Stub for easier conversion functions generalization. */
template <typename T>
constexpr T
Convert_le_8(T x)
{
    return x;
}

/** Convert 16 bits value byte order from LE to host byte order and vice versa.
 * Template parameters:
 * - *T* 16-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_le_16(T x)
{
    return Is_system_le() ? x : VSM_BSWAP16(x);
}

/** Convert 32 bits value byte order from LE to host byte order and vice versa.
 * Template parameters:
 * - *T* 32-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_le_32(T x)
{
    return Is_system_le() ? x : VSM_BSWAP32(x);
}

/** Convert 64 bits value byte order from LE to host byte order and vice versa.
 * Template parameters:
 * - *T* 64-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_le_64(T x)
{
    return Is_system_le() ? x : VSM_BSWAP64(x);
}

/** Stub for easier conversion functions generalization. */
template <typename T>
constexpr T
Convert_nh_8(T x)
{
    return x;
}

/** Convert 16 bits value byte order from network to host byte order and vice
 * versa.
 * Template parameters:
 * - *T* 16-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_nh_16(T x)
{
    return Convert_be_16(x);
}

/** Convert 32 bits value byte order from network to host byte order and vice
 * versa.
 * Template parameters:
 * - *T* 32-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_nh_32(T x)
{
    return Convert_be_32(x);
}

/** Convert 64 bits value byte order from network to host byte order and vice
 * versa.
 * Template parameters:
 * - *T* 64-bits integer type.
 * @param x Value to convert.
 */
template <typename T>
constexpr T
Convert_nh_64(T x)
{
    return Convert_be_64(x);
}

/** @{ */
/** Definitions for byte order conversions for all integer types. */
#define __VSM_BO_INT(type_size)     int ## type_size ## _t
#define __VSM_BO_UINT(type_size)    uint ## type_size ## _t

#define _VSM_DEF_BO_CONV(type_size, type_name) \
    /** Convert value from network to host byte order. */ \
    constexpr type_name \
    Ntoh(type_name x) \
    { \
        return Convert_nh_ ## type_size (x); \
    } \
    \
    /** Convert value from host to network byte order. */ \
    constexpr type_name \
    Hton(type_name x) \
    { \
        return Convert_nh_ ## type_size (x); \
    } \
    \
    /** Convert value from LE to host byte order and vice versa. */ \
    constexpr type_name \
    Le(type_name x) \
    { \
        return Convert_le_ ## type_size (x); \
    } \
    \
    /** Convert value from BE to host byte order and vice versa. */ \
    constexpr type_name \
    Be(type_name x) \
    { \
        return Convert_be_ ## type_size (x); \
    } \

#define VSM_DEF_BO_CONV(type_size) \
    _VSM_DEF_BO_CONV(type_size, __VSM_BO_INT(type_size)) \
    _VSM_DEF_BO_CONV(type_size, __VSM_BO_UINT(type_size))

/** @} */

VSM_DEF_BO_CONV(8)
VSM_DEF_BO_CONV(16)
VSM_DEF_BO_CONV(32)
VSM_DEF_BO_CONV(64)

/** Convert float type from host to network format. */
constexpr float
Hton(float x)
{
    //XXX
    return x;
}

/** Convert float type from network to host format. */
constexpr float
Ntoh(float x)
{
    //XXX
    return x;
}

/** Convert double type from host to network format. */
constexpr double
Hton(double x)
{
    //XXX
    return x;
}

/** Convert double type from network to host format. */
constexpr double
Ntoh(double x)
{
    //XXX
    return x;
}

/** Convert float type from LE to host format. */
constexpr float
Le(float x)
{
    //XXX
    return x;
}

/** Convert float type from BE to host format. */
constexpr float
Be(float x)
{
    //XXX
    return x;
}

/** Convert float type from LE to host format. */
constexpr double
Le(double x)
{
    //XXX
    return x;
}

/** Convert float type from BE to host format. */
constexpr double
Be(double x)
{
    //XXX
    return x;
}

#ifndef NO_DOXYGEN
namespace internal {

/** Helper class for LE-order conversions. */
class Le_converter {
public:
    template <typename T>
    static constexpr T
    Convert(T value)
    {
        return Le(value);
    }
};

/** Helper class for BE-order conversions. */
class Be_converter {
public:
    template <typename T>
    static constexpr T
    Convert(T value)
    {
        return Be(value);
    }
};

} /* namespace internal */
#endif

/** Helper class for byte-order-dependent value representation. */
template <typename T, class Converter>
class Bo_value {
public:
    /** Construct value.
     *
     * @param value Value in host byte order.
     */
    Bo_value(T value):
        value(Converter::Convert(value))
    {}

    /** Assign new value.
     *
     * @param value Value in host byte order.
     */
    Bo_value &
    operator =(T value)
    {
        this->value = Converter::Convert(value);
        return *this;
    }

    /** Cast to underlying type.
     *
     * @return Value in host byte order.
     */
    operator T() const
    {
        return Converter::Convert(value);
    }

    /** Get the value of underlying type.
     *
     * @return Value in host byte order.
     */
    T
    Get() const
    {
        return Converter::Convert(value);
    }

private:
    /** Stored value (in wire byte order). */
    T value;
} __PACKED;

/** Little-endian value wrapper.
 * @param T Underlying primitive type.
 */
template <typename T>
using Le_value = Bo_value<T, internal::Le_converter>;

/** Big-endian value wrapper.
 * @param T Underlying primitive type.
 */
template <typename T>
using Be_value = Bo_value<T, internal::Be_converter>;

// @{
/** Standard primitive types for little-endian byte order. */
typedef Le_value<int8_t> Le_int8;
typedef Le_value<uint8_t> Le_uint8;
typedef Le_value<int16_t> Le_int16;
typedef Le_value<uint16_t> Le_uint16;
typedef Le_value<int32_t> Le_int32;
typedef Le_value<uint32_t> Le_uint32;
typedef Le_value<float> Le_float;
typedef Le_value<double> Le_double;
// @}


// @{
/** Standard primitive types for big-endian byte order. */
typedef Be_value<int8_t> Be_int8;
typedef Be_value<uint8_t> Be_uint8;
typedef Be_value<int16_t> Be_int16;
typedef Be_value<uint16_t> Be_uint16;
typedef Be_value<int32_t> Be_int32;
typedef Be_value<uint32_t> Be_uint32;
typedef Be_value<float> Be_float;
typedef Be_value<double> Be_double;
// @}

} /* namespace vsm */
} /* namespace ugcs */

#endif /* VSM_ENDIAN_H_ */
