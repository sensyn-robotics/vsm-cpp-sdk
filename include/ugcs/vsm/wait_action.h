// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file wait_action.h
 */
#ifndef _UGCS_VSM_WAIT_ACTION_H_
#define _UGCS_VSM_WAIT_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>
#include <chrono>

namespace ugcs {
namespace vsm {

/** Wait action. Hold at the current position for specified amount of time.
 * Implementation depends on vehicle type. */
class Wait_action: public Action {
    DEFINE_COMMON_CLASS(Wait_action, Action)

public:
    /** Construct wait action explicitly. */
    Wait_action(double wait_time) :
        Action(Type::WAIT),
        wait_time(wait_time) {}

    /** Time to wait in seconds. */
    double wait_time;
};

/** Type mapper for wait action. */
template<>
struct Action::Mapper<Action::Type::WAIT> {
    /** Real type. */
    typedef Wait_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_WAIT_ACTION_H_*/
