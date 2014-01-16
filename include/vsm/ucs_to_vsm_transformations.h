// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file ucs_to_vsm_transformations.h
 */
#ifndef _UCS_TO_VSM_TRANSFORMATIONS_H_
#define _UCS_TO_VSM_TRANSFORMATIONS_H_

#include <vsm/action.h>
#include <vsm/mavlink.h>

namespace vsm {

/** Transformations from UCS data representation to user visible (i.e. VSM)
 * data structures.
 */
class Ucs_to_vsm_transformations {
public:

    /** Thrown when some value is unsupported and could not be transformed/understood. */
    VSM_DEFINE_EXCEPTION(Unsupported_exception);

    /** Parse extended Mavlink mission item and return appropriate action.
     * @throw Unsupported_exception if Mavlink mission item command is unsupported.
     * @throw vsm::Action::Format_exception if internal action representation is
     *        wrong.
     */
    static Action::Ptr
    Parse_mission_item_ex(const mavlink::ugcs::Pld_mission_item_ex& item);
};

} /* namespace vsm */

#endif /* _UCS_TO_VSM_TRANSFORMATIONS_H_ */
