// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * Implementation of Mavlink demultiplexer.
 */

#include <ugcs/vsm/mavlink_demuxer.h>

using namespace ugcs::vsm;

constexpr Mavlink_demuxer::Message_id Mavlink_demuxer::MESSAGE_ID_ANY;

constexpr Mavlink_demuxer::System_id Mavlink_demuxer::SYSTEM_ID_ANY;

constexpr Mavlink_demuxer::Component_id Mavlink_demuxer::COMPONENT_ID_ANY;

void
Mavlink_demuxer::Disable()
{
    default_handler = Default_handler();
    std::unique_lock<std::mutex> lock(mutex);
    handlers.clear();
}

void
Mavlink_demuxer::Register_default_handler(Default_handler handler)
{
    default_handler = handler;
}

bool
Mavlink_demuxer::Demux(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
                       System_id system_id, uint8_t component_id)
{
    if (Demux_try(buffer, message_id, system_id, component_id)) {
        return true;
    }
    if (!default_handler) {
        return false;
    }
    if (default_handler(message_id, system_id, component_id)) {
        return Demux_try(buffer, message_id, system_id, component_id);
    }
    return false;
}

void
Mavlink_demuxer::Unregister_handler(Key& key)
{
    ASSERT(key);
    std::unique_lock<std::mutex> lock(mutex);
    VERIFY(handlers.erase(key), 1);
    key.Reset();
}

bool
Mavlink_demuxer::Demux_try(
        Io_buffer::Ptr buffer,
        mavlink::MESSAGE_ID_TYPE message_id,
        System_id system_id,
        uint8_t component_id)
{
    /* Try exact match first. */
    if (Demux_try_one(buffer, message_id, system_id, component_id,
            system_id, component_id)) {
        return true;
    }

    /* Try all components for specific system. */
    if (Demux_try_one(buffer, message_id, system_id, COMPONENT_ID_ANY,
            system_id, component_id)) {
        return true;
    }

    /* Try specific component for any system. */
    if (Demux_try_one(buffer, message_id, SYSTEM_ID_ANY, component_id,
            system_id, component_id)) {
        return true;
    }

    /* Finally try any system and any component. */
    if (Demux_try_one(buffer, message_id, SYSTEM_ID_ANY, COMPONENT_ID_ANY,
            system_id, component_id)) {
        return true;
    }

    return false;
}

bool
Mavlink_demuxer::Demux_try_one(Io_buffer::Ptr buffer,
                               mavlink::MESSAGE_ID_TYPE message_id,
                               System_id system_id,
                               Component_id component_id,
                               System_id real_system_id,
                               uint8_t real_component_id)
{
    Callback_base::Ptr cb;

    Key key(message_id, system_id, component_id);
    {
        std::unique_lock<std::mutex> lock(mutex);
        auto iter = handlers.find(key);
        if (iter == handlers.end()) {
            return false;
        }

        cb = iter->second;
    }

    (*cb)(buffer, real_system_id, real_component_id);

    return true;
}


