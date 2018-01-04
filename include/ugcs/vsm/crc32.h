// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>
#include <stddef.h>

namespace ugcs {
namespace vsm {

class Crc32 {
public:
    void
    Reset()
    {
        crc = 0xFFFFFFFF;
    }

    uint32_t
    Add_byte(uint8_t b)
    {
        crc = crc_tab[(crc ^ b) & 0xff] ^ (crc >> 8);
        return ~crc;
    }

    uint32_t
    Add_short(uint16_t b)
    {
        return Add_buffer(&b, 2);
    }

    uint32_t
    Add_int(uint32_t b)
    {
        return Add_buffer(&b, 4);
    }

    uint32_t
    Add_buffer(void* b, size_t len)
    {
        auto buf = static_cast<uint8_t*>(b);
        for (; len; --len, ++buf) {
            crc = crc_tab[(crc ^ (*buf)) & 0xff] ^ (crc >> 8);
        }
        return ~crc;
    }

    uint32_t
    Get()
    {
        return ~crc;
    }

private:
    uint32_t crc = 0xFFFFFFFF;
    static uint32_t crc_tab[];
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _CRC32_H_ */
