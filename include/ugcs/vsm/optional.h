// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file optional.h
 *
 * Implementation for Optional class.
 */

#ifndef _OPTIONAL_H_
#define _OPTIONAL_H_

namespace ugcs {
namespace vsm {

/** Type for null option. Used to disengage optional value. */
struct Nullopt_t {};

/** Null option. Used to disengage optional value. */
constexpr Nullopt_t nullopt;

/** The class encapsulates optional value of type T. */
template <typename T>
class Optional {
public:

    constexpr Optional():
        /* Initialize storage to prevent from false GCC warnings about
         * uninitialized value used in release build.
         */
        storage{0}, is_valid(false)
    {}

    constexpr Optional(Nullopt_t):
        storage{0}, is_valid(false)
    {}

    Optional(const T &value):
        is_valid(true)
    {
        new(storage) T(value);
    }

    Optional(T &&value):
        is_valid(true)
    {
        new(storage) T(std::move(value));
    }

    Optional(const Optional &value):
        is_valid(value.is_valid)
    {
        if (is_valid) {
            new(storage) T(*value);
        }
    }

    Optional(Optional &&value):
        is_valid(value.is_valid)
    {
        if (is_valid) {
            new(storage) T(std::move(*value));
        }
    }

    ~Optional()
    {
        if (is_valid) {
            (**this).~T();
        }
    }

    Optional &
    operator =(Nullopt_t)
    {
        Disengage();
    }

    Optional &
    operator =(const T &value)
    {
        if (is_valid) {
            **this = value;
        } else {
            new(storage) T(value);
            is_valid = true;
        }
        return *this;
    }

    Optional &
    operator =(T &&value)
    {
        if (is_valid) {
            **this = std::move(value);
        } else {
            new(storage) T(std::move(value));
            is_valid = true;
        }
        return *this;
    }

    Optional &
    operator =(const Optional &value)
    {
        if (is_valid) {
            if (value.is_valid) {
                **this = *value;
            } else {
                (**this).~T();
                is_valid = false;
            }
        } else if (value.is_valid) {
            new(storage) T(*value);
            is_valid = true;
        }
        return *this;
    }

    Optional &
    operator =(Optional &&value)
    {
        if (is_valid) {
            if (value.is_valid) {
                **this = *std::move(value);
            } else {
                (**this).~T();
                is_valid = false;
            }
        } else if (value.is_valid) {
            new(storage) T(*std::move(value));
            is_valid = true;
        }
        return *this;
    }

    template <typename U,
              typename = typename std::enable_if<std::is_constructible<T, U>::value>::type,
              typename = typename std::enable_if<std::is_assignable<T, U>::value>::type>
    Optional &
    operator =(U &&value)
    {
        if (is_valid) {
            (**this) = std::forward<U>(value);
        } else {
            new(storage) T(std::forward<U>(value));
            is_valid = true;
        }
        return *this;
    }

    T &
    operator *() &
    {
        ASSERT(is_valid);
        T* tmp = reinterpret_cast<T *>(storage);
        return *tmp;
    }

    constexpr
    const T &
    operator *() const &
    {
        ASSERT(is_valid);
        const T * tmp = reinterpret_cast<const T *>(storage);
        return *tmp;
    }

    T &&
    operator *() &&
    {
        ASSERT(is_valid);
        T* tmp = reinterpret_cast<T *>(storage);
        return std::move(*tmp);
    }

    T *
    operator ->() &
    {
        return &(**this);
    }

    constexpr
    const T *
    operator ->() const &
    {
        return &(**this);
    }

    constexpr explicit operator bool() const
    {
        return is_valid;
    }

    void
    Disengage()
    {
        if (is_valid) {
            (**this).~T();
            is_valid = false;
        }
    }

private:
    union {
        /** Storage for the value. */
        uint8_t storage[sizeof(T)];
        /** Keep the value aligned. */
        long align;
    };
    bool is_valid;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _OPTIONAL_H_ */
