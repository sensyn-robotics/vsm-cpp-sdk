// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file  mavlink_demuxer.h
 *
 * Mavlink demultiplexer based on message, system and component
 * identifiers.
 */
#ifndef _MAVLINK_DEMUXER_H_
#define _MAVLINK_DEMUXER_H_

#include <ugcs/vsm/mavlink_decoder.h>
#include <ugcs/vsm/request_context.h>
#include <unordered_map>

namespace ugcs {
namespace vsm {

/** Mavlink message demultiplexer based on message, system and component
 * identifiers. Supposed to be conveniently used with @ref Mavlink_decoder.
 * Messages from different mavlink extensions but with same identifiers can
 * not be demultiplexed simultaneously.
 */
class Mavlink_demuxer {
public:
    /** Helper type for Mavlink message id which is able to hold special values.
     */
    typedef signed Message_id;

    /** Special value representing any Mavlink message id. */
    static constexpr Message_id MESSAGE_ID_ANY = -1;

    /** Helper type for Mavlink system id which is able to hold special values.
     */
    typedef int64_t System_id;

    /** Special value representing any Mavlink system id. */
    static constexpr System_id SYSTEM_ID_ANY = -1;

    /** Helper type for Mavlink component id which is able to hold special values.
     */
    typedef signed Component_id;

    /** Special value representing any Mavlink component id. */
    static constexpr Component_id COMPONENT_ID_ANY = -1;

    /** Thrown when handler with a given filtering criteria is already registered. */
    VSM_DEFINE_EXCEPTION(Duplicate_handler);

    /** Handler type for the specific demultiplexed Mavlink message. */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extention_type = mavlink::Extension>
    using Handler = Callback_proxy<
            void, typename mavlink::Message<message_id, Extention_type>::Ptr>;

    /** Default handler which is called for all Mavlink messages which does
     * not have a handler.
     * Io_buffer contains the raw payload data.
     * @return @a true message will be resubmitted to demuxer for further
     * processing, otherwise @a false.
     */
    typedef Callback_proxy<bool, Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE, System_id, uint8_t, uint32_t>
        Default_handler;

    /** Convenience builder for Mavlink demuxer default handlers. */
    DEFINE_CALLBACK_BUILDER(Make_default_handler,
            (Io_buffer::Ptr, mavlink::MESSAGE_ID_TYPE, System_id, uint8_t, uint32_t),
            (Io_buffer::Ptr(nullptr), mavlink::MESSAGE_ID::DEBUG_VALUE, mavlink::SYSTEM_ID_NONE, 0, 0))

    /** Convenience builder for Mavlink demuxer handlers. */
    DEFINE_CALLBACK_BUILDER_TEMPLATE(Make_handler,
            (mavlink::MESSAGE_ID_TYPE message_id, class Extention_type),
            (typename mavlink::Message<message_id, Extention_type>::Ptr),
            (typename mavlink::Message<message_id, Extention_type>::Ptr(nullptr)))

    /** Mavlink message handler key. Extention type is not taken into account. */
    class Key {
    public:
        /** Construct non-empty key. */
        Key(mavlink::MESSAGE_ID_TYPE message_id, System_id system_id, Component_id component_id) :
            message_id(message_id),
            system_id(system_id),
            component_id(component_id) {
        }

        /** Construct empty key. */
        Key() {}

        /** Hasher class for a key type. */
        class Hasher {
        public:
            /** Calculate hash value. */
            size_t
            operator()(const Key& key) const
            {
                return key.message_id ^ (key.system_id << 16) ^ (key.component_id << 24);
            }
        };

        /** Invalidate key. */
        void
        Reset()
        {
            id = 0;
        }

        /** Check for key validness. */
        explicit operator bool() const
        {
            return id != 0;
        }

        /** Equality operator. */
        bool
        operator ==(const Key& key) const
        {
            return this->message_id == key.message_id &&
                   this->system_id == key.system_id &&
                   this->component_id == key.component_id;
        }

    private:
        mavlink::MESSAGE_ID_TYPE message_id;

        System_id system_id;

        Component_id component_id;

        static std::atomic_int generator;
        int id = 0;

        // used only when inserting into handlers.
        void
        Generate_id()
        {
            id = std::atomic_fetch_add(&Key::generator, 1);
        }

        friend class Mavlink_demuxer;
    };

    /** Default constructor. */
    Mavlink_demuxer() = default;

    /** Delete copy constructor. */
    Mavlink_demuxer(const Mavlink_demuxer&) = delete;

    /** Should be called prior to intention to delete the instance. */
    void
    Disable();

    /** Register default handler. Called when more specific handler does not
     * exist. Always called from the thread, which calls @ref Demux method.*/
    void
    Register_default_handler(Default_handler handler);

    /** Register handler for specific Mavlink message, system id and
     * component id.
     * @param handler Handler taking specific Mavlink message.
     * @param system_id System id to call the handler for, or @ref SYSTEM_ID_ANY
     * to call the handler for any system id.
     * @param component_id Component id to call the handler for, or
     * @ref COMPONENT_ID_ANY to call the handler for any component id.
     * @param processor If given, specifies request processor in which context
     * the handler should be executed, otherwise handler is executed from the
     * thread which calls @ref Demux method.
     * @return Valid registration key which can be used to unregister the
     * handler later.
     */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extention_type>
    Key
    Register_handler(
            Handler<message_id, Extention_type> handler,
            System_id system_id = SYSTEM_ID_ANY,
            Component_id component_id = COMPONENT_ID_ANY,
            Request_processor::Ptr processor = nullptr)
    {
        auto callback = Callback<message_id, Extention_type>::Create(
                handler, processor);
        Key key(message_id, system_id, component_id);
        key.Generate_id();
        std::unique_lock<std::mutex> lock(mutex);
        handlers.insert(std::make_pair(key, std::move(callback)));
        return key;
    }

    /** Demultiplex Mavlink message.
     * @return @a true if message was handled by some non-default handler,
     *      otherwise @a false.
     */
    bool
    Demux(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
          System_id system_id, uint8_t component_id, uint32_t request_id);

    /** Unregister handler using registration key. Key is invalidated upon exit
     * from the method. */
    void
    Unregister_handler(Key&);

private:
    /** Demultiplex the message based on user provided identifiers. This method
     * tries to find the best handler match based on system and component identifiers.
     * @return @a true if some handler was found and called, otherwise @a false.
     */
    bool
    Demux_try(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
              System_id system_id, uint8_t component_id, uint32_t request_id);

    /** Demultiplex the message based on provided identifiers which could have
     * special values.
     * @return @a true if some handler was found and called, otherwise @a false.
     */
    bool
    Demux_try_one(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
                  System_id system_id, Component_id component_id,
                  System_id real_system_id,
                  uint8_t real_component_id,
                  uint32_t request_id);

    /** Callback base class to provide a unified interface to convert raw
     * data buffer to specific Mavlink message and call associated
     * handler.
     */
    class Callback_base: public std::enable_shared_from_this<Callback_base> {
        DEFINE_COMMON_CLASS(Callback_base, Callback_base)
    public:
        Callback_base(Request_processor::Ptr processor) :
            processor(processor) {}

        virtual
        ~Callback_base()
        {};

        virtual void
        operator()(Io_buffer::Ptr buffer, System_id system_id,
                    uint8_t component_id, uint32_t request_id) = 0;

    protected:
        /** Optional request processor for a handler (may be nullptr). */
        Request_processor::Ptr processor;
    };

    /** Callback for specific Mavlink message with necessary payload building. */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extention_type>
    class Callback: public Callback_base {
        DEFINE_COMMON_CLASS(Callback, Callback_base)

    public:
        /** Specific message type of this callback. */
        using Message_type = mavlink::Message<message_id, Extention_type>;

        Callback(Handler<message_id, Extention_type> handler,
                Request_processor::Ptr processor):
            Callback_base(processor),
            handler(handler)
        {}

        virtual void
        operator()(Io_buffer::Ptr buffer, System_id system_id,
                    uint8_t component_id, uint32_t request_id) override
        {
            typename Message_type::Ptr message =
                    Message_type::Create(system_id, component_id, request_id, buffer);
            if (processor) {
                /* Callback will be invoked from processor context. */
                auto request = Request::Create();
                request->Set_processing_handler(Make_callback(&Callback::Invoke,
                        Shared_from_this(), std::move(message), request));
                processor->Submit_request(std::move(request));

            } else {
                /* Invoke from the calling thread. */
                handler(message);
            }
        }

    private:
        /** Handler of the specific message. */
        Handler<message_id, Extention_type> handler;

        /** Invoke the handler. */
        void
        Invoke(typename Message_type::Ptr message, Request::Ptr request)
        {
            handler(message);
            if (request) {
                request->Complete();
            }
        }
    };

    /** Default handler for unregistered messages. */
    Default_handler default_handler;

    /** Handlers for specific Mavlink messages. */
    std::unordered_multimap<Key, Callback_base::Ptr, Key::Hasher> handlers;

    /** Mutex should be acquired when reading/writing handlers. */
    std::mutex mutex;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _MAVLINK_DEMUXER_H_ */
