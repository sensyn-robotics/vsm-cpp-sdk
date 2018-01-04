// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file vtol_transition_action.h
 */
#ifndef _VTOL_TRANSITION_ACTION_H_
#define _VTOL_TRANSITION_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** Wait action. Hold at the current position for specified amount of time.
 * Implementation depends on vehicle type. */
class Vtol_transition_action: public Action {
    DEFINE_COMMON_CLASS(Vtol_transition_action, Action)

public:
    /** VTOL transition state */
    enum Mode {
        /** VTOL mode. */
        VTOL,
        /** Fixed-wing mode. */
        FIXED
    };

    /** Construct wait action explicitly. */
    Vtol_transition_action(Vtol_transition_action::Mode mode) :
        Action(Type::VTOL_TRANSITION),
        mode(mode) {}

    Mode mode;
};

/** Type mapper for wait action. */
template<>
struct Action::Mapper<Action::Type::VTOL_TRANSITION> {
    /** Real type. */
    typedef Vtol_transition_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _VTOL_TRANSITION_ACTION_H_*/
