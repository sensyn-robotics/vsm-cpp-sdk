// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file io_buffer.h
 *
 * Io_buffer class implementation.
 */

#include <ugcs/vsm/utils.h>

#include <memory>
#include <vector>

#ifndef _IO_BUFFER_H_
#define _IO_BUFFER_H_

namespace ugcs {
namespace vsm {

/** Generic I/O buffer. Used for all I/O operations with network, filesystem,
 * peripheral devices. Should always be passed by Io_buffer::Ptr smart pointer
 * type. This allows anyone to hold reference to the buffer until he needs to
 * access data and release the buffer when it last reference released.
 *
 * Important: the buffer object is immutable - once it is created its data
 * cannot be modified. It is required not confuse other reference holders. If
 * modified buffer is required it can be easily created based on existing one
 * via different constructors, operators and methods. Create() method should be
 * used for obtaining Io_buffer instance.
 */
class Io_buffer: public std::enable_shared_from_this<Io_buffer> {
    DEFINE_COMMON_CLASS(Io_buffer, Io_buffer)

public:
    /** Special value which references data end. */
    static const size_t END;

    /** Copy constructor.
     *
     * @param buf Buffer to copy from.
     * @param offset Offset in bytes to reference data in the source buffer.
     * @param len Length in bytes of the referenced data. Value END indicates
     *          that all remaining data are taken.
     * @throws Invalid_param_exception if the specified offset or length exceeds
     *      the source buffer boundary.
     */
    Io_buffer(const Io_buffer &buf, size_t offset = 0, size_t len = END);

    /** Move constructor. */
    Io_buffer(Io_buffer &&buf);

    /** Construct from data provided in vector. Typical usage is for data which
     * are firstly received from some I/O call.
     *
     * @param data_vec Vector with data. Should be dynamically allocated by
     *      "std::make_shared()". Keep in mind that it can be passed somewhere
     *      else only by shared pointer.
     * @param offset Offset in data vector where this buffer data start from.
     * @param len Length of the data referenced in the provided vector. Offset
     *      and length should not exceed vector boundary. Value END indicates
     *      that all remaining data in vector should be used.
     * @throws Invalid_param_exception if the specified offset or length exceeds
     *      the source buffer boundary.
     */
    explicit Io_buffer(std::shared_ptr<const std::vector<uint8_t>> &&data_vec,
                       size_t offset = 0, size_t len = END);

    /** Construct from data provided in vector. See constructor which accepts
     * "std::shared_ptr<const std::vector<uint8_t>> &&" for more details.
     */
    Io_buffer(std::vector<uint8_t> &&data_vec, size_t offset = 0, size_t len = END);

    /** Construct empty buffer. */
    Io_buffer();

    /** Construct buffer from string. Null terminator is not included in the data.
     * Data are copied.
     */
    Io_buffer(const std::string &data_str);

    /** Construct buffer from provided bytes array. Data are copied.
     *
     * @param data Bytes array with data.
     * @param len Number of bytes to take from data argument.
     */
    Io_buffer(const void *data, size_t len);

    ~Io_buffer();

    /** Concatenate this buffer data with another buffer data and return new
     * buffer object which contains resulted data. Note, that method complexity
     * is linear if lengths of both buffers are not zero.
     *
     * @param buf Buffer to concatenate with.
     * @return New buffer with data from this buffer and the specified one.
     */
    Ptr
    Concatenate(Io_buffer::Ptr buf);

    /** Take slice from buffer data
     *
     * @param offset Offset to start slice from.
     * @param len Length of the slice data. Value END indicates that all
     *      remaining data should be taken.
     * @return New buffer with sliced data.
     * @throws Invalid_param_exception if the specified offset or length exceeds
     *      the source buffer boundary.
     */
    Ptr
    Slice(size_t offset, size_t len = END) const;

    /** Get length of the contained data. */
    size_t
    Get_length() const
    {
        return len;
    }

    /** Get pointer to raw data stored in the buffer.
     * @throws Invalid_op_exception if the buffer is empty.
     */
    const void *
    Get_data() const;

    /** Get buffer content as string. */
    std::string
    Get_string() const;

    /** Get buffer data as string of characters. Non-printable chars are substituted with '.' */
    std::string
    Get_ascii() const;

    /** Get buffer data as hex string */
    std::string
    Get_hex() const;

private:
    /** Data contained in the buffer. The buffer may reference only part of them.
     * Null pointer if the buffer is empty.
     * @see Io_buffer::len
     * @see Io_buffer::offset
     */
    std::shared_ptr<const std::vector<uint8_t>> data;
    /** Data offset in "data" member. */
    size_t offset;
    /** Data length of the chunk in "data" member. */
    size_t len;

    /** Internal constructor for copy/slice operations.
     *
     * @param data_vec Vector with data.
     * @param offset Offset in data vector where this buffer data start from.
     * @param len Length of the data referenced in the provided vector. Offset
     *      and length should not exceed vector boundary. Value END indicates
     *      that all remaining data in vector should be used.
     * @throws Invalid_param_exception if the specified offset or length exceeds
     *      the source buffer boundary.
     */
    Io_buffer(const std::shared_ptr<const std::vector<uint8_t>> &data_vec,
              size_t offset = 0, size_t len = END);

    /** Internal creation function for copy/slice operations. */
    static Ptr
    Create(const std::shared_ptr<const std::vector<uint8_t>> &data_vec,
           size_t offset = 0, size_t len = END);

    /** Initialize attributes for data vector. */
    void
    Init_data(size_t offset, size_t len);
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _IO_BUFFER_H_ */
