// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file payload_steering_action.h
 *
 * Payload steering action definition.
 */
#ifndef _PAYLOAD_STEERING_ACTION_H_
#define _PAYLOAD_STEERING_ACTION_H_

#include <vsm/action.h>

namespace vsm {

/** Steer the vehicle payload. */
class Payload_steering_action: public Action {
    DEFINE_COMMON_CLASS(Payload_steering_action, Action)

public:

    Payload_steering_action() :
        Action(Type::PAYLOAD_STEERING) {}
};

/** Type mapper for payload steering action. */
template<>
struct Action::Mapper<Action::Type::PAYLOAD_STEERING> {
    /** Real type. */
    typedef Payload_steering_action type;
};

} /* namepsace vsm */

#endif /* _PAYLOAD_STEERING_ACTION_H_ */
