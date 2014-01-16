// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Description:
 *  Io_buffer class implementation.
 */

#include <vsm/io_buffer.h>
#include <vsm/exception.h>

#include <cstring>

using namespace vsm;

const size_t Io_buffer::END = -1;

Io_buffer::Ptr
Io_buffer::Create(const std::shared_ptr<const std::vector<uint8_t>> &data_vec,
                  size_t offset, size_t len)
{
    /* Cannot use std::make_shared here since the constructor is private. */
    Io_buffer *ptr = new Io_buffer(data_vec, offset, len);
    try {
        return std::shared_ptr<Io_buffer>(ptr);
    } catch (...) {
        delete ptr;
        throw;
    }
}

Io_buffer::~Io_buffer()
{
}

Io_buffer::Io_buffer():
    data(nullptr), offset(0), len(0)
{
}

Io_buffer::Io_buffer(const Io_buffer &buf, size_t offset, size_t len):
    std::enable_shared_from_this<vsm::Io_buffer>(buf),
    data(buf.data)
{
    if (len == END) {
        if (offset > buf.len) {
            VSM_EXCEPTION(Invalid_param_exception, "Offset is too large");
        }
    } else if (offset + len > buf.len) {
        VSM_EXCEPTION(Invalid_param_exception,
                      "Offset and length exceeds buffer boundary");
    }
    this->offset = buf.offset + offset;
    if (len == END) {
        this->len = buf.len - offset;
    } else {
        this->len = len;
    }
    if (len == 0) {
        data = nullptr;
    }
}

Io_buffer::Io_buffer(Io_buffer &&buf):
    data(std::move(buf.data)), offset(buf.offset), len(buf.len)
{
}

Io_buffer::Io_buffer(const std::shared_ptr<const std::vector<uint8_t>> &data_vec,
                     size_t offset, size_t len):
    data(data_vec)
{
    Init_data(offset, len);
}

Io_buffer::Io_buffer(std::shared_ptr<const std::vector<uint8_t>> &&data_vec,
                     size_t offset, size_t len):
    data(std::move(data_vec))
{
    if (!data.unique()) {
        VSM_EXCEPTION(Invalid_param_exception, "Passed buffer pointer is not unique");
    }
    Init_data(offset, len);
}

Io_buffer::Io_buffer(std::vector<uint8_t> &&data_vec, size_t offset, size_t len):
    data(std::make_shared<std::vector<uint8_t>>(std::move(data_vec)))
{
    Init_data(offset, len);
}

void
Io_buffer::Init_data(size_t offset, size_t len)
{
    if (len == END) {
        if (offset > data->size()) {
            VSM_EXCEPTION(Invalid_param_exception, "Offset is too large");
        }
    } else if (offset + len > data->size()) {
        VSM_EXCEPTION(Invalid_param_exception,
                      "Offset and length exceeds vector boundary");
    }
    this->offset = offset;
    if (len == END) {
        this->len = data->size() - offset;
    } else {
        this->len = len;
    }
    if (len == 0) {
        data = nullptr;
    }
}

Io_buffer::Io_buffer(const std::string &data_str)
{
    offset = 0;
    len = data_str.size();
    const char *buf = data_str.c_str();
    data = std::make_shared<const std::vector<uint8_t>>(buf, buf + len);
}

Io_buffer::Io_buffer(const void *data, size_t len)
{
    if (len) {
        this->data = std::make_shared<const std::vector<uint8_t>>(
            static_cast<const char *>(data),
            static_cast<const char *>(data) + len);
    }
    this->offset = 0;
    this->len = len;
}

Io_buffer::Ptr
Io_buffer::Concatenate(Io_buffer::Ptr buf)
{
    if (buf->len == 0) {
        return Shared_from_this();
    }
    if (len == 0) {
        return buf;
    }
    auto vec = std::make_shared<std::vector<uint8_t>>(len + buf->len);
    std::memcpy(&vec->front(), &(*data)[offset], len);
    std::memcpy(&(*vec)[len], &(*buf->data)[buf->offset], buf->len);
    return Create(std::move(vec));
}

Io_buffer::Ptr
Io_buffer::Slice(size_t offset, size_t len) const
{
    if (offset > this->len) {
        VSM_EXCEPTION(Invalid_param_exception,
                      "Offset exceeds buffer boundary");
    }
    if (len == END) {
        len = this->len - offset;
    } else if (offset + len > this->len) {
        VSM_EXCEPTION(Invalid_param_exception,
                      "Offset and length exceeds buffer boundary");
    }
    if (len == 0) {
        return Create(nullptr, 0);
    }
    return Create(data, this->offset + offset, len);
}

const void *
Io_buffer::Get_data() const
{
    if (len == 0)
        return nullptr;
    else
        return &(*data)[offset];
}

std::string
Io_buffer::Get_string() const
{
    if (len == 0) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char *>(&data->front() + offset), len);
}

std::string
Io_buffer::Get_ascii() const
{
    auto ret = std::string();
    for(size_t a = 0; a < len; a++)
    {
        char c = (*data)[a];
        if (c < 32) c = '.';
        ret += c;
    }
    return ret;
}

std::string
Io_buffer::Get_hex() const
{
    const char h[] = "0123456789abcdef";
    auto ret = std::string();
    for(size_t i = offset; i < offset + len; i++)
    {
        char c = (*data)[i];
        ret += h[((c >> 4) & 15)];
        ret += h[(c & 15)];
        ret += ' ';
    }
    return ret;
}
