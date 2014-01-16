// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** @file utils.h
 * @file utils.h
 *
 * Various common utilities
 */
#ifndef _UTILS_H_
#define _UTILS_H_

#include <vsm/platform.h>
#include <vsm/platform_sockets.h>
#include <vsm/debug.h>
#include <vsm/exception.h>

#include <type_traits>
#include <vsm/regex.h>

/** Use this macro to define some common attributes for a class.
 * @param __class_name Name for the class being defined.
 * @param ... Name of the class used as a template parameter when deriving from
 *      std::enable_shared_from_this.
 */
#define DEFINE_COMMON_CLASS(__class_name, ...) \
    public: \
    \
    /** Pointer type */ \
    typedef std::shared_ptr<__class_name> Ptr; \
    \
    /** Pointer type */ \
    typedef std::weak_ptr<__class_name> Weak_ptr; \
    \
    /** Create an instance. */ \
    template <typename... Args> \
    static Ptr \
    Create(Args &&... args) \
    { \
        return std::make_shared<__class_name>(std::forward<Args>(args)...); \
    } \
    \
    private: \
    typedef vsm::internal::Shared_getter<__class_name, ## __VA_ARGS__> Shared_getter; \
    friend Shared_getter; \
    /** Return shared pointer of this class instance. Must not be  called from */ \
    /** the constructor. */ \
    Ptr \
    Shared_from_this() \
    { \
        return Shared_getter::Get(this); \
    }

namespace vsm {

namespace internal {

/** Helper class for working with classes which are derived from
 * std::enable_shared_from_this.
 */
template <class T, class Shared_base = void>
class Shared_getter {
public:
    /** The type of base class. */
    typedef Shared_base Shared_base_type;

    /** Get shared pointer to derived class. */
    static std::shared_ptr<T>
    Get(T *this_ptr)
    {
        return std::dynamic_pointer_cast<T>(this_ptr->shared_from_this());
    }
};

/** Specialization for class which is not derived from std::enable_shared_from_this. */
template <class T>
class Shared_getter<T, void> {
public:
    /** Get which leads to runtime failure. */
    static std::shared_ptr<T>
    Get(T *this_ptr __UNUSED)
    {
        /* Should not be called. Missing shared base class argument for
         * DEFINE_COMMON_CLASS if fired.
         */
        ASSERT(false);
        return std::shared_ptr<T>();
    }
};

/** Specialization for class which is directly derived from
 * std::enable_shared_from_this.
 */
template <class T>
class Shared_getter<T, T> {
public:
    /** Get shared pointer to itself. */
    static std::shared_ptr<T>
    Get(T *this_ptr)
    {
        return this_ptr->shared_from_this();
    }
};

} /* namespace internal */

/**
 * This flag is used to support case insensitive file systems in regular expression matching.
 * defined as regex::regex_constants::icase on windows  (Ignore case when matching patterns)
 * defined as regex::regex_constants::none on linux (All matching is case sensitive)
 */
extern regex::regex_constants::syntax_option_type platform_independent_filename_regex_matching_flag;

namespace utils {

    //XXX platform-dependent code! should be moved
    /**
     * Makes handle to be non-blocking.
     *
     * @param handle Handle to make non-blocking.
     */
    void
    Make_nonblocking_handle(platform::Socket_handle handle);

    //XXX platform-dependent code! should be moved
    /**
     * Ignore specific Unix signal.
     * @param signal_number Signal number.
     */
    void
    Ignore_signal(int signal_number);

} /* namespace utils */

} /* namespace vsm */

#endif /* _UTILS_H_ */
