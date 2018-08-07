// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file reference_guard.h
 *
 * Reference guard class definition.
 */

#ifndef _UGCS_VSM_REFERENCE_GUARD_H_
#define _UGCS_VSM_REFERENCE_GUARD_H_

#include <ugcs/vsm/defs.h>

#include <algorithm>
#include <type_traits>
#include <functional>

namespace ugcs {
namespace vsm {

/** Reference guard objects keep references for managed objects. The managed
 * object should have Add_ref() and Release_ref() methods which are called
 * by the guarded object. Release_ref() can tear down the object when last
 * reference is released. Both Add_ref() and Release_ref() should be thread-safe.
 * @param Class_ptr Type for referenced object pointer. It can be either plain
 *      pointer or any smart pointer class.
 */
template <typename Class_ptr>
class Reference_guard {
public:
    /** Construct an empty guard which doesn't have managed object. */
    Reference_guard(std::nullptr_t ptr = nullptr):
        ptr(ptr)
    {}

    /** Construct guard object. */
    Reference_guard(const Class_ptr &ptr):
        ptr(ptr)
    {
        if (this->ptr) {
            this->ptr->Add_ref();
        }
    }

    /** Construct guard by moving managed object to the guard. */
    Reference_guard(Class_ptr &&ptr):
        ptr(std::move(ptr))
    {
        if (this->ptr) {
            this->ptr->Add_ref();
        }
    }

    /** Copy guard object. */
    Reference_guard(const Reference_guard<Class_ptr> &ref):
        ptr(ref.ptr)
    {
        if (ptr) {
            ptr->Add_ref();
        }
    }

    /** Move guard object. */
    Reference_guard(Reference_guard<Class_ptr> &&ref):
        ptr(std::move(ref.ptr))
    {
        ref.ptr = nullptr;
    }

    /** Copy guard object. */
    template <typename Src_class_ptr>
    Reference_guard(const Reference_guard<Src_class_ptr> &ref):
        ptr(ref.ptr)
    {
        if (ptr) {
            ptr->Add_ref();
        }
    }

    /** Move guard object. */
    template <typename Src_class_ptr>
    Reference_guard(Reference_guard<Src_class_ptr> &&ref):
        ptr(std::move(ref.ptr))
    {
        ref.ptr = nullptr;
    }

    ~Reference_guard()
    {
        if (ptr) {
            ptr->Release_ref();
        }
    }

    /** Copy assignment. */
    Reference_guard<Class_ptr> &
    operator =(const Reference_guard<Class_ptr> &ref)
    {
        if (ref.ptr) {
            ref.ptr->Add_ref();
        }
        if (ptr) {
            ptr->Release_ref();
        }
        ptr = ref.ptr;
        return *this;
    }

    /** Move assignment. */
    Reference_guard<Class_ptr> &
    operator =(Reference_guard<Class_ptr> &&ref)
    {
        if (ptr) {
            ptr->Release_ref();
        }
        ptr = std::move(ref.ptr);
        ref.ptr = nullptr;
        return *this;
    }

    /** nullptr assignment. */
    Reference_guard<Class_ptr> &
    operator =(std::nullptr_t ref __UNUSED)
    {
        if (ptr) {
            ptr->Release_ref();
        }
        ptr = nullptr;
        return *this;
    }

    /** Guards are equal when managed object pointers are equal. */
    bool
    operator ==(const Reference_guard<Class_ptr> &ref) const
    {
        return ptr == ref.ptr;
    }

    /** Guards are not equal when managed object pointers are not equal. */
    bool
    operator !=(const Reference_guard<Class_ptr> &ref) const
    {
        return ptr != ref.ptr;
    }

    /** Guards yields to true when managed object does exist. */
    explicit operator bool() const
    {
        return ptr != nullptr;
    }

    /** Member access is transparent. */
    Class_ptr
    operator ->()
    {
        ASSERT(ptr);
        return ptr;
    }

    /** Member access is transparent. */
    const Class_ptr
    operator ->() const
    {
        ASSERT(ptr);
        return ptr;
    }

    /** Dereference is transparent. */
    decltype(*std::declval<Class_ptr>())
    operator *()
    {
        return *ptr;
    }

#ifndef NO_DOXYGEN
    /** Dereference is transparent. */
    decltype(*std::declval<Class_ptr>())
    operator *() const
    {
        return *ptr;
    }
#endif

    /** Hasher class for reference type. */
    class Hasher {
    public:
        /** Calculate hash value. */
        size_t
        operator()(const Reference_guard& ref) const
        {
            static std::hash<Class_ptr> hasher;
            return hasher(ref.ptr);
        }
    };

private:
    template <typename> friend class Reference_guard;
    /** Pointer to the associated object. */
    Class_ptr ptr;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* _UGCS_VSM_REFERENCE_GUARD_H_ */
