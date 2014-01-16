// Copyright (c) 2014, Smart Projects Holdings Ltd
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

#include <vsm/mavlink_decoder.h>
#include <unordered_map>
#include <vsm/request_context.h>

namespace vsm {

/** Mavlink message demultiplexer based on message, system and component
 * identifiers. Supposed to be conveniently used with @ref Mavlink_decoder.
 * Messages from different mavlink extentions but with same identifiers can
 * not be demultiplexed simulataneously.
 */
class Mavlink_demuxer {
public:
    /** Thrown when handler with a given filtering criteria is already registered. */
    VSM_DEFINE_EXCEPTION(Duplicate_handler);
    /** Handler type for the specific demultiplexed Mavlink message. */
    template<mavlink::MESSAGE_ID_TYPE message_id, class Extention_type = mavlink::Extension>
    using Handler = Callback_proxy<
            void, typename mavlink::Message<message_id, Extention_type>::Ptr>;

    /** Default handler which is called for all Mavlink messages which does
     * not have a handler.
     * @return @a true message will be resubmitted to demuxer for further
     * processing, otherwise @a false.
     */
    typedef Callback_proxy<bool, mavlink::MESSAGE_ID_TYPE, mavlink::System_id, uint8_t>
        Default_handler;

    /** Helper type for Mavlink message id which is able to hold special values.
     */
    typedef signed Message_id;

    /** Special value representing any Mavlink message id. */
    static constexpr Message_id MESSAGE_ID_ANY = -1;

    /** Helper type for Mavlink system id which is able to hold special values.
     */
    typedef signed System_id;

    /** Special value representing any Mavlink system id. */
    static constexpr System_id SYSTEM_ID_ANY = -1;

    /** Helper type for Mavlink component id which is able to hold special values.
     */
    typedef signed Component_id;

    /** Special value representing any Mavlink component id. */
    static constexpr Component_id COMPONENT_ID_ANY = -1;

    /** Mavlink message handler key. Extention type is not taken into account. */
    class Key {
    public:
        /** Construct non-empty key. */
        Key(mavlink::MESSAGE_ID_TYPE message_id, System_id system_id,
                Component_id component_id) :
                    message_id(message_id), system_id(system_id),
                    component_id(component_id), valid(true) {}

        /** Construct empty key. */
        Key() : valid(false) {}

        /** Hasher class for a key type. */
        class Hasher {
        public:

            /** Calculate hash value. */
            size_t
            operator ()(const Key& key) const
            {
                static_assert(sizeof(key.message_id) == sizeof(uint8_t), "Please adjust bit shift!");
                return static_cast<uint8_t>(key.message_id) ^
                        key.system_id << 8 ^
                        key.component_id << 16;
            }
        };

        /** Invalidate key. */
        void
        Reset()
        {
            valid = false;
        }

        /** Check for key validness. */
        explicit operator bool() const
        {
            return valid;
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

        bool valid;

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
     * @throw Duplicate_handler if handler for the given filtering combination
     * is already registered.
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
        std::unique_lock<std::mutex> lock(mutex);
        auto result = handlers.insert(std::make_pair(key, std::move(callback)));
        if (!result.second) {
            VSM_EXCEPTION(Duplicate_handler, "Duplicate handler for message id "
                    "[%d] system id [%d] component id [%d]",
                    message_id, system_id, component_id);
        }
        return key;
    }

    /** Demultiplex Mavlink message.
     * @return @a true if message was handled by some non-default handler,
     *      otherwise @a false.
     */
    bool
    Demux(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
          mavlink::System_id system_id, uint8_t component_id);

#ifndef AR_DRONE_REDESIGNED
    /** Unregister Mavlink message handler based on system, message and
     * component identifiers, which could have also special values.
     */
    void
    Unregister_handler(System_id system_id = SYSTEM_ID_ANY,
                       Message_id message_id = MESSAGE_ID_ANY,
                       Component_id component_id = COMPONENT_ID_ANY);
#endif

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
              mavlink::System_id system_id, uint8_t component_id);

    /** Demultiplex the message based on provided identifiers which could have
     * special values.
     * @return @a true if some handler was found and called, otherwise @a false.
     */
    bool
    Demux_try_one(Io_buffer::Ptr buffer, mavlink::MESSAGE_ID_TYPE message_id,
                  System_id system_id, Component_id component_id,
                  mavlink::System_id real_system_id,
                  uint8_t real_component_id);

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
        operator ()(Io_buffer::Ptr buffer, mavlink::System_id system_id,
                    uint8_t component_id) = 0;

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
        operator ()(Io_buffer::Ptr buffer, mavlink::System_id system_id,
                    uint8_t component_id) override
        {
            typename Message_type::Ptr message =
                    Message_type::Create(system_id, component_id, buffer);
            if (processor) {
                /* Callback will be invoked from processor context. */
                auto request = Request::Create();
                request->Set_processing_handler(Make_callback(&Callback::Invoke,
                        Shared_from_this(), std::move(message), request));
                processor->Submit_request(std::move(request));

            } else {
                /* Invoke from the calling thread. */
                Invoke(std::move(message));
            }
        }

    private:
        /** Handler of the specific message. */
        Handler<message_id, Extention_type> handler;

        /** Invoke the handler. */
        void
        Invoke(typename Message_type::Ptr message, Request::Ptr request = nullptr)
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
    std::unordered_map<Key, Callback_base::Ptr, Key::Hasher> handlers;

    /** Mutex should be acquired when reading/writing handlers. */
    std::mutex mutex;
};

/** Convenience builder for Mavlink demuxer default handlers. */
DEFINE_CALLBACK_BUILDER(Make_mavlink_demuxer_default_handler,
        (mavlink::MESSAGE_ID_TYPE, mavlink::System_id, uint8_t),
        (mavlink::MESSAGE_ID::DEBUG_VALUE, mavlink::SYSTEM_ID_NONE, 0))

/** Convenience builder for Mavlink demuxer handlers. */
DEFINE_CALLBACK_BUILDER_TEMPLATE(Make_mavlink_demuxer_handler,
        (mavlink::MESSAGE_ID_TYPE message_id, class Extention_type),
        (typename mavlink::Message<message_id, Extention_type>::Ptr),
        (typename mavlink::Message<message_id, Extention_type>::Ptr(nullptr)))


} /* namespace vsm */

#endif /* _MAVLINK_DEMUXER_H_ */
