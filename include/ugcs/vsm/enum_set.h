// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file enum_set.h
 */
#ifndef _ENUM_SET_H_
#define _ENUM_SET_H_

#include <bitset>

namespace ugcs {
namespace vsm {

/** Convenient set of enum values. Enum_type should have field named LAST as
 * a last enum item which is used to know the size of the enum.
 */
template<typename Enum_type>
class Enum_set {
public:

    /** Construct the set based on arbitrary set of values. */
    template<typename... Enum_types>
    Enum_set(Enum_types... values)
    {
        Set_recursive(values...);
    }

    /** Set enum value presence to true or false. */
    void
    Set(Enum_type value, bool present = true)
    {
        set.set(static_cast<long>(value), present);
    }

    /** Reset state to empty. */
    void
    Reset()
    {
        set.reset();
    }

    /** Check if enum value is present or not. */
    bool
    Is_set(Enum_type value) const
    {
        return set.test(static_cast<long>(value));
    }

    /** Equality operator. */
    bool
    operator==(const Enum_set& other) const
    {
        return set == other.set;
    }

private:

    /** Traverse enum values one by one. */
    template<typename... Enum_types>
    void
    Set_recursive(Enum_type value, Enum_types... values)
    {
        Set(value);
        Set_recursive(values...);
    }

    /** End of recursion. */
    void
    Set_recursive()
    {

    }

    /** Container. */
    std::bitset<static_cast<long>(Enum_type::LAST)> set;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _ENUM_SET_H_ */
