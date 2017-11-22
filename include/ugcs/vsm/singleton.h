// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file singleton.h
 *
 * Singleton class definition.
 */

#ifndef SINGLETON_H_
#define SINGLETON_H_

#include <memory>
#include <mutex>

namespace ugcs {
namespace vsm {

namespace internal {

/** Helper class for singletons instantiation. It deals with classes both having
 * and not having default constructor.
 */
template <class T, class = void>
class Singleton_creator {
public:
    /** Get singleton instance for a class which does not have default
     * constructor. It is never created without arguments, so return null.
     */
    static std::shared_ptr<T>
    Create()
    {
        return nullptr;
    }

    /** Create singleton instance.
     *
     * @param args Arguments for singleton class constructor.
     * @return Pointer to the created object.
     */
    template <typename... Args>
    static std::shared_ptr<T>
    Create(Args &&...args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

/** Specialization for class which has default constructor. */
template <class T>
class Singleton_creator<T, typename std::enable_if<std::is_default_constructible<T>::value>::type> {
public:
    /** Always can create an instance. */
    template <typename... Args>
    static std::shared_ptr<T>
    Create(Args &&...args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

} /* namespace internal */

/** Helper class for implementing singletons.
 * @param T type for instantiated class.
 */
template <class T>
class Singleton {
public:
    /** Disable copying. */
    Singleton(const Singleton &) = delete;

    Singleton() = default;

    /** Get singleton instance. The same instance is returned until the last
     * reference is released. In case the target class does not have default
     * constructor and the constructor arguments are not provided, the new
     * instance is not created - nullptr is returned if it was not previously
     * created.
     *
     * @param args Arguments to constructor if any.
     * @return Global singleton instance.
     */
    template <typename... Args>
    std::shared_ptr<T>
    Get_instance(Args &&...args)
    {
        auto ptr = instance.lock();
        if (!ptr) {
            ptr = internal::Singleton_creator<T>::Create(std::forward<Args>(args)...);
            instance = ptr;
        }
        return ptr;
    }

private:
    /** Global instance. */
    std::weak_ptr<T> instance;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* SINGLETON_H_ */
