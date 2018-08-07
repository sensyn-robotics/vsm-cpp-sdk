// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file param_setter.h
 *
 * Parameters setter.
 */

#ifndef _UGCS_VSM_PARAM_SETTER_H_
#define _UGCS_VSM_PARAM_SETTER_H_

#include <ugcs/vsm/callback.h>

namespace ugcs {
namespace vsm {

namespace param_setter_internal {

/** Callable setter object. */
template <typename... Params>
class Param_setter {
public:
    /** Construct the setter.
     *
     * @param params Variables which will be linked to the setter object.
     *      References to them are stored in the setter and value is modified
     *      when the setter is called.
     */
    Param_setter(Params &... params):
        params(params...)
    {}

    /** Set values to the linked variables.
     *
     * @param values Values to set. Should correspond to the variables list
     *      provided to the constructor.
     */
    void
    operator()(Params... values)
    {
        params = std::tuple<Params...>(values...);
    }

private:
    /** Stored reference to parameters to set. */
    std::tuple<typename std::add_lvalue_reference<Params>::type...> params;
};

} /* namespace param_setter_internal */

/** Make setter callback for provided parameters. It can be used as a convenience
 * callback which just sets all provided parameters to the arguments values when
 * invoked.
 * @param params References to the parameters storage.
 * @return Callback object which returns void type and accepts values for the
 *      provided parameters.
 */
template <typename... Params>
typename Callback<param_setter_internal::Param_setter<Params...>, void, Params...>::Ptr
Make_setter(Params &... params)
{
    return Make_callback(param_setter_internal::Param_setter<Params...>(params...),
                         Params()...);
}

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_PARAM_SETTER_H_ */
