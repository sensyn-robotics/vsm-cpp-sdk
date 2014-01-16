// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file mavlink_stream.h
 */
#ifndef _MAVLINK_STREAM_H_
#define _MAVLINK_STREAM_H_

#include <vsm/io_stream.h>
#include <vsm/mavlink_decoder.h>
#include <vsm/mavlink_demuxer.h>
#include <vsm/mavlink_encoder.h>
#include <tuple>
#include <queue>

namespace vsm {

/** Convenience class for interpreting an I/O stream as a stream of
 * Mavlink messages. It is assumed, that only one such class at a time is
 * used with a given I/O stream.
 */
class Mavlink_stream: public std::enable_shared_from_this<Mavlink_stream>
{
    DEFINE_COMMON_CLASS(Mavlink_stream, Mavlink_stream)

public:

    /** Construct Mavlink stream using a I/O stream. */
    Mavlink_stream(
            Io_stream::Ref stream,
            const mavlink::Extension& extension = mavlink::ugcs::Extension::Get());

    /** Disable copy constructor. */
    Mavlink_stream(const Mavlink_stream&) = delete;

    /** Get underlying I/O stream. */
    Io_stream::Ref&
    Get_stream();

    /** Get underlying decoder. */
    Mavlink_decoder&
    Get_decoder();

    /** Get underlying demuxer. */
    Mavlink_demuxer&
    Get_demuxer();

    /** Bind decoder and demuxer, so that output of the decoder is automatically
     * passed to the demuxer.
     */
    void
    Bind_decoder_demuxer();

    /** Send Mavlink message to other end asynchronously. Timeout should be
     * always present, otherwise there is a chance to overflow the write queue
     * if underlying stream is write-blocked. Only non-temporal completion
     * contexts are allowed. */
    void
    Send_message(
            const mavlink::Payload_base& payload,
            mavlink::System_id system_id,
            uint8_t component_id,
            const std::chrono::milliseconds& timeout,
            Operation_waiter::Timeout_handler,
            const Request_completion_context::Ptr& completion_ctx);

    /** Disable the class. Underlying I/O stream is freed, but not explicitly
     * closed, because this stream could be passed for further processing.
     * Unfinished write operations are aborted.
     */
    void
    Disable();

private:

    /** Underlying stream. */
    Io_stream::Ref stream;

    /** Decoder used with a stream. */
    Mavlink_decoder decoder;

    /** Demuxer used with a stream. */
    Mavlink_demuxer demuxer;

    /** Encoder used with a stream. */
    Mavlink_encoder encoder;

    /** Removes completed write operations from the top of the queue. */
    void
    Cleanup_write_ops();

    /** Currently scheduled write operations. Some of them could be already
     * finished (i.e. done), so Cleanup_write_ops() should be called periodically
     * to remove them from the queue.
     */
    std::queue<Operation_waiter> write_ops;
};

} /* namespace vsm */

#endif /* _MAVLINK_STREAM_H_ */
