// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_encoder.h
 *
 * Encoder capable of creating byte buffers based on Mavlink payload
 * and identifiers.
 */
#ifndef _MAVLINK_ENCODER_H_
#define _MAVLINK_ENCODER_H_

#include <vsm/io_buffer.h>
#include <vsm/mavlink.h>

namespace vsm {

/** XXX */
class Mavlink_encoder {
public:
    /** Encode Mavlink message.
     * @param payload Payload.
     * @param system_id System id.
     * @param component_id Component id.
     * @return Byte buffer ready to be directly written on to the wire.
     */
    Io_buffer::Ptr
    Encode(const mavlink::Payload_base& payload, mavlink::System_id system_id,
           uint8_t component_id);

private:
    /** Current sequence number. */
    uint8_t seq = 0;
};

} /* namespace vsm */

#endif /* _MAVLINK_ENCODER_H_ */
