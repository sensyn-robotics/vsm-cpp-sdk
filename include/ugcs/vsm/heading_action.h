// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file heading_action.h
 */
#ifndef _UGCS_VSM_HEADING_ACTION_H_
#define _UGCS_VSM_HEADING_ACTION_H_

#include <ugcs/vsm/action.h>
#include <ugcs/vsm/mavlink.h>

namespace ugcs {
namespace vsm {

/** Change the heading of a vehicle. Heading is an angle between the North and
 * head (nose) of the vehicle. If vehicle can't change the heading without
 * changing the movement vector, then payload heading should be changed instead.
 */
class Heading_action: public Action {
    DEFINE_COMMON_CLASS(Heading_action, Action)

public:
    /** Construct the heading action explicitly. */
    Heading_action(double heading) :
        Action(Type::HEADING),
        heading(heading) {}

    /**
     * Heading in radians.
     */
    double heading;
};

/** Type mapper for Heading action. */
template<>
struct Action::Mapper<Action::Type::HEADING> {
    /** Real type. */
    typedef Heading_action type;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_HEADING_ACTION_H_ */
