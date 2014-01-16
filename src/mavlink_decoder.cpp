// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of Mavlink decoder.
 */
#include <vsm/mavlink_decoder.h>
#include <vsm/debug.h>

using namespace vsm;

void
Mavlink_decoder::Disable()
{
    handler = Handler();
}

void
Mavlink_decoder::Reset(bool reset_stats)
{
    state = State::STX;
    payload = nullptr;
    header.clear();
    checksum.clear();
    if (reset_stats) {
    	stats = Stats();
    }
}

size_t
Mavlink_decoder::Get_next_read_size() const
{
	const mavlink::Header* header_ptr;

	switch (state) {
	/*
	 * In STX and HEADER states we don't want to read any payload data,
	 * because we want to receive the whole payload at once together with
	 * checksum to avoid memory copy.
	 */
	case State::STX: return sizeof(mavlink::Header);
	case State::HEADER: return sizeof(mavlink::Header) - header.size();
	case State::PAYLOAD:
		header_ptr =
				reinterpret_cast<const mavlink::Header*>(header.data());
		return header_ptr->payload_len - payload->Get_length() + sizeof(uint16_t);
	case State::CHECKSUM: return sizeof(uint16_t) - checksum.size();
	default:
		ASSERT(false);
		VSM_EXCEPTION(Internal_error_exception, "Unexpected state %d", state);
	}
}

const Mavlink_decoder::Stats &
Mavlink_decoder::Get_stats() const
{
	return stats;
}

void
Mavlink_decoder::Decode(Io_buffer::Ptr buffer)
{
    stats.bytes_received += buffer->Get_length();
    while (buffer->Get_length()) {
        switch (state) {
        case State::STX:
            buffer = Decode_stx(buffer);
            break;
        case State::HEADER:
            buffer = Decode_header(buffer);
            break;
        case State::PAYLOAD:
            buffer = Decode_payload(buffer);
            break;
        case State::CHECKSUM:
            buffer = Decode_checksum(buffer);
            break;
        }
    }
}

Io_buffer::Ptr
Mavlink_decoder::Decode_stx(Io_buffer::Ptr buffer)
{
    const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
    size_t len = buffer->Get_length();

    for ( ; len ; len--, data++) {
        if (*data == mavlink::START_SIGN) {
            state = State::HEADER;
            header.clear();
            break;
        }
    }

    return buffer->Slice(buffer->Get_length() - len);
}

Io_buffer::Ptr
Mavlink_decoder::Decode_header(Io_buffer::Ptr buffer)
{
    const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
    size_t len = buffer->Get_length();

    for ( ; len && header.size() < sizeof(mavlink::Header) ; len--, data++) {
        header.push_back(*data);
    }

    if (header.size() == sizeof(mavlink::Header)) {
        state = State::PAYLOAD;
        payload = Io_buffer::Create();
    }

    return buffer->Slice(buffer->Get_length() - len);
}

Io_buffer::Ptr
Mavlink_decoder::Decode_payload(Io_buffer::Ptr buffer)
{
    mavlink::Header* header_ptr = reinterpret_cast<mavlink::Header*>(header.data());

    size_t to_read = std::min(header_ptr->payload_len - payload->Get_length(),
                              buffer->Get_length());

    payload = payload->Concatenate(buffer->Slice(0, to_read));

    if (payload->Get_length() == header_ptr->payload_len) {
        state = State::CHECKSUM;
        checksum.clear();
    }

    return buffer->Slice(to_read);
}

Io_buffer::Ptr
Mavlink_decoder::Decode_checksum(Io_buffer::Ptr buffer)
{
    const uint8_t* data = static_cast<const uint8_t*>(buffer->Get_data());
    size_t len = buffer->Get_length();

    for ( ; len && checksum.size() < sizeof(uint16_t) ; len--, data++) {
        checksum.push_back(*data);
    }

    if (checksum.size() == sizeof(uint16_t)) {
        mavlink::Header* header_ptr = reinterpret_cast<mavlink::Header*>(header.data());

        mavlink::Checksum sum(&header[1], header.size() - 1);
        sum.Accumulate(payload);
        try {
            uint16_t sum_calc =
                    sum.Accumulate(sum.Get_extra_byte(
                            header_ptr->message_id, extension));
            /* Convert checksum in Mavlink byte order to a host byte order
             * compatible type. */
            mavlink::Uint16* sum_recv = reinterpret_cast<mavlink::Uint16*>(checksum.data());

            if (sum_calc == *sum_recv) {
                mavlink::MESSAGE_ID_TYPE message_id = header_ptr->message_id.Get();
                if (handler) {
                	handler(payload, message_id, header_ptr->system_id, header_ptr->component_id);
                    stats.handled++;
                } else {
                	stats.no_handler++;
                    LOG_INFO("Mavlink message handler not registered.");
                }

            } else {
            	stats.bad_checksum++;
                LOG_DEBUG("Mavlink checksum mismatch recv=%x calc=%x",
                          sum_recv->Get(), sum_calc);
            }

        } catch (mavlink::Checksum::Invalid_id_exception &) {
        	stats.unknown_id++;
            LOG_DEBUG("Unknown Mavlink message id: %d", header_ptr->message_id.Get());
        }

        state = State::STX;
    }

    return buffer->Slice(buffer->Get_length() - len);
}
