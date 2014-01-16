// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <vsm/mavlink_stream.h>

using namespace vsm;

Mavlink_stream::Mavlink_stream(
        Io_stream::Ref stream,
        const mavlink::Extension& extension) :
                stream(stream), decoder(extension)
{

}

Io_stream::Ref&
Mavlink_stream::Get_stream()
{
    return stream;
}

Mavlink_decoder&
Mavlink_stream::Get_decoder()
{
    return decoder;
}

Mavlink_demuxer&
Mavlink_stream::Get_demuxer()
{
    return demuxer;
}

void
Mavlink_stream::Bind_decoder_demuxer()
{
    auto binder = [](Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
            mavlink::System_id system_id, uint8_t component_id,
            Mavlink_stream::Ptr mav_stream)
    {
        mav_stream->demuxer.Demux(buffer, message_id, system_id, component_id);
    };

    decoder.Register_handler(Make_mavlink_decoder_handler(binder, Shared_from_this()));
}

void
Mavlink_stream::Send_message(
        const mavlink::Payload_base& payload,
        mavlink::System_id system_id,
        uint8_t component_id,
        const std::chrono::milliseconds& timeout,
        Operation_waiter::Timeout_handler timeout_handler,
        const Request_completion_context::Ptr& completion_ctx)
{
    ASSERT(completion_ctx->Get_type() != Request_completion_context::Type::TEMPORAL);

    Io_buffer::Ptr buffer = encoder.Encode(payload, system_id, component_id);

    Operation_waiter waiter = stream->Write(
            buffer,
            Make_dummy_callback<void, Io_result>(),
            completion_ctx);
    waiter.Timeout(timeout, timeout_handler, true, completion_ctx);

    write_ops.emplace(std::move(waiter));
    Cleanup_write_ops();
}

void
Mavlink_stream::Disable()
{
    decoder.Disable();
    demuxer.Disable();
    stream = nullptr;
    while (!write_ops.empty()) {
        write_ops.front().Abort();
        write_ops.pop();
    }
}

void
Mavlink_stream::Cleanup_write_ops()
{
    while (!write_ops.empty()) {
        if (write_ops.front().Is_done()) {
            write_ops.pop();
            continue;
        }
        /* Operations are completed sequentially. */
        break;
    }
}
