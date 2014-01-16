// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/** @file callback.h
 * Generic callback which can be used to define and create an instance
 * of an abstract callable operation with arbitrary arguments. The main benefit
 * is that callback can be executed without knowing anything about the arguments
 * of the associated operation. For example, vsm::Request class uses callbacks to
 * notify request handlers about the completion of a particular request.
 */

#ifndef _CALLBACK_H_
#define _CALLBACK_H_

#include <vsm/defs.h>
#include <vsm/exception.h>
#include <vsm/debug.h>

#include <tuple>
#include <memory>
#include <functional>

/** Internal helper for unpacking parenthesis-enclosed parameters lists. */
#define __UNPACK_PARAMS(...)    __VA_ARGS__

/** @def DEFINE_CALLBACK_BUILDER
 * Define callback builder function. Use when forced arguments required.
 * @param __name Name for a created function.
 * @param __types Types of forced arguments (parenthesis-enclosed comma-separated
 *      list).
 * @param __values Initial values for the forced arguments (parenthesis-enclosed
 *      comma-separated list).
 *
 * Usage example:
 * @code
 * DEFINE_CALLBACK_BUILDER(Make_data_handler,
 *                         (vsm::Io_result, vsm::Io_buffer::Ptr),
 *                         (vsm::Io_result::OK, nullptr));
 * @endcode
 */
#define DEFINE_CALLBACK_BUILDER(__name, __types, __values) \
      template <class __Callable, typename... __Args> \
      __DEFINE_CALLBACK_BUILDER_BODY(__name, __types, __values)

/** @def DEFINE_CALLBACK_BUILDER_TEMPLATE
 * The same as DEFINE_CALLBACK_BUILDER, but allows specifying additional
 * template parameters for builders which arguments depend on templates.
 * @param __name See DEFINE_CALLBACK_BUILDER
 * @param __template Additional template parameters (parenthesis-enclosed
 *      comma-separated list).
 * @param __types See DEFINE_CALLBACK_BUILDER
 * @param __values See DEFINE_CALLBACK_BUILDER
 */
#define DEFINE_CALLBACK_BUILDER_TEMPLATE(__name, __template, __types, __values) \
      template <__UNPACK_PARAMS __template, class __Callable, typename... __Args> \
      __DEFINE_CALLBACK_BUILDER_BODY(__name, __types, __values)

#ifndef NO_DOXYGEN
/* Common part of DEFINE_CALLBACK_BUILDER and DEFINE_CALLBACK_BUILDER_TEMPLATE. */
#define __DEFINE_CALLBACK_BUILDER_BODY(__name, __types, __values) \
    static \
    typename vsm::Callback_forced_args<__Callable, \
                                       std::tuple<__UNPACK_PARAMS __types>, \
                                       __Args...>::Callback_type::Ptr \
    __name(__Callable &&__callable, __Args &&... __args) \
    { \
        return vsm::Callback_forced_args<__Callable, \
                                         std::tuple<__UNPACK_PARAMS __types>, \
                                         __Args...>::Create( \
            std::forward<__Callable>(__callable), \
            std::forward_as_tuple<__UNPACK_PARAMS __types>(__UNPACK_PARAMS __values), \
            std::forward<__Args>(__args)...); \
    }
#endif

namespace vsm {

/** Callback which can be executed. Custom callbacks should be derived from this
 * class.
 * @param Result Return value type of the callback execution.
 * @see Callback
 */
template<typename Result>
class Callback_base {
public:
    /** Result type. */
    typedef Result Result_t;

    /** Pointer class type. It is callable for convenient callback invocation.*/
    template <class Callback = Callback_base>
    class Ptr: public std::shared_ptr<Callback> {
    public:
        /** Type of target callback. */
        typedef Callback Callback_type;

        /** Constructor accepts the same arguments as std::shared_ptr. */
        template <typename... Args>
        Ptr(Args &&... args):
            std::shared_ptr<Callback_type>(std::forward<Args>(args)...)
        {}

        /** Call the associated callback.
         * @throws Nullptr_exception if the pointer is null.
         */
        Result_t
        operator ()()
        {
            if (!std::shared_ptr<Callback_type>::get()) {
                VSM_EXCEPTION(Nullptr_exception,
                              "Attempted to invoke empty callback");
            }
            return (*std::shared_ptr<Callback_type>::get())();
        }
    };

    /** Execute callback. */
    virtual Result_t
    operator ()() = 0;
};

#ifndef NO_DOXYGEN
/** Black magic is here. Do not open! Brain damage possible. */
namespace callback_internal {

/** Sequence of argument indices is represented by this helper type. The
 * sequence is used when unpacking arguments tuple.
 */
template <int...>
struct Sequence
{};

/** Sequence generator helper type. Used for generating sequence with arguments
 * indices.
 */
template<int n, int... s>
struct Sequence_generator: Sequence_generator<n - 1, n - 1, s...>
{};

/** Specialization for the base class with the actual sequence. */
template<int... s>
struct Sequence_generator<0, s...>
{
    /** Generated sequence type. */
    typedef Sequence<s...> Sequence_type;
};

/** Adapt parameter type to the form suitable for storing in callback arguments
 * pack.
 */
template <typename T>
struct Adapt_arg_type {
    /** Just remove reference and CV-qualifiers in generic version. */
    typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
};

/** Specialization for characters array which is assumed to be string literal. */
template <size_t size>
struct Adapt_arg_type<const char (&)[size]> {
    /** Transform to pointer. */
    typedef const char *type;
};

/** Adapt callable parameter type to the form suitable for storing in callback
 * object.
 */
template <typename T, typename Enable = void>
struct Adapt_callable_type {
    /** Just remove reference and CV-qualifiers in generic version. */
    typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
};

/** Specialization for function. */
template <typename F>
struct Adapt_callable_type<F,
        typename std::enable_if<
            std::is_function<
                typename std::remove_reference<F>::type>::value>::type> {
    /** Preserve as is. */
    typedef F type;
};

/** Evaluated to true is callable object is a class method (versus lambda or
 * function). */
template <class Method>
struct Is_method_ptr_type {
    static constexpr bool value =
        std::is_member_function_pointer<typename callback_internal::Adapt_callable_type<Method>::type>::value;
};

/** Check if the provided type is pointer to non-static class method. */
template <class Method>
constexpr bool
Is_method_ptr()
{
    return Is_method_ptr_type<Method>::value;
}

} /* namespace callback_internal */
#endif

/** Generic callback. Use {@link Make_callback} for convenient
 * instantiation. Generic template version for any callable object.
 *
 * @param Callable Any callable object type (function, lambda, class with
 *      overloaded call operator etc.).
 * @param Enable Dummy template argument for conditional specialization.
 * @param Args Arguments pack.
 * @see Make_callback
 */
template <class Callable, class Enable, typename... Args>
class Callback:
    public Callback_base<typename std::result_of<Callable(Args...)>::type> {
public:
    /** Base class type. */
    typedef Callback_base<typename std::result_of<Callable(Args...)>::type>
        Base_type;
    /** Callable pointer class type. */
    typedef typename Base_type::template Ptr<Callback> Ptr;

private:
    /** Type for stored arguments tuple. */
    typedef std::tuple<typename callback_internal::Adapt_arg_type<Args>::type...> Args_tuple;

public:
    /** Get type of the specified argument.
     * @param arg_idx Index of the argument. Should be in range [0, sizeof...(Args)).
     */
    template <size_t arg_idx>
    using Arg_type = typename std::tuple_element<arg_idx, Args_tuple>::type;

    /** Create callback instance. */
    static Ptr
    Create(Callable &&callable, Args &&... args)
    {
        return std::make_shared<Callback>(std::forward<Callable>(callable),
                                          std::forward<Args>(args)...);
    }

    /** Construct callback instance.
     *
     * @param callable Callable entity instance.
     * @param args User defined arguments pack.
     */
    Callback(Callable &&callable, Args &&... args):
        callable(std::forward<Callable>(callable)),
        args(std::forward<Args>(args)...)
    {}

    /** Move constructor. */
    Callback(Callback &&) = default;

    /** Execute callback. */
    virtual typename Base_type::Result_t
    operator ()() override
    {
        return Invoke(typename callback_internal::Sequence_generator
                        <sizeof...(Args)>::Sequence_type());
    }

    /** Get reference to an argument at the specified position.
     * Template parameters:
     * - *arg_idx* Index of the argument to get.
     * @return Reference to the stored argument.
     */
    template <size_t arg_idx>
    Arg_type<arg_idx> &
    Get_arg()
    {
        return std::get<arg_idx>(args);
    }

protected:
    /** Callable entity from user. */
    typename callback_internal::Adapt_callable_type<Callable>::type callable;
    /** User provided arguments pack. */
    Args_tuple args;

    /** Invoke callable entity with user provided arguments. */
    template <int... s>
    typename Base_type::Result_t
    Invoke(callback_internal::Sequence<s...>)
    {
        return callable(std::get<s>(args)...);
    }
};

/** Generic callback which uses bound class method as callable. Use
 * {@link Make_callback} for convenient instantiation. Specialized template
 * version for class member function.
 *
 * @param Class_ptr Pointer to class object (used with pointer semantic, can
 *      be any smart pointer as well).
 * @param Class method to call.
 * @param Args Arguments pack.
 * @see Make_callback
 */
template <class Method, class Class_ptr, typename... Args>
class Callback<Method,
               typename std::enable_if<callback_internal::Is_method_ptr<Method>()>::type,
               Class_ptr, Args...>:
    public Callback_base<typename std::result_of<Method(Class_ptr, Args...)>::type> {
public:
    /** Base class type. */
    typedef Callback_base<typename std::result_of<Method(Class_ptr, Args...)>::type>
        Base_type;
    /** Callable pointer class type. */
    typedef typename Base_type::template Ptr<Callback> Ptr;

private:
    /** Type for stored arguments tuple. */
    typedef std::tuple<typename callback_internal::Adapt_arg_type<Args>::type...> Args_tuple;

public:
    /** Get type of the specified argument.
     * @param arg_idx Index of the argument. Should be in range [0, sizeof...(Args)).
     */
    template <size_t arg_idx>
    using Arg_type = typename std::tuple_element<arg_idx, Args_tuple>::type;

    /** Create callback instance. */
    static Ptr
    Create(Method method, Class_ptr &&obj_ptr, Args &&... args)
    {
        return std::make_shared<Callback>(method,
                                          std::forward<Class_ptr>(obj_ptr),
                                          std::forward<Args>(args)...);
    }

    /** Constructor for class method bound callback. */
    Callback(Method method, Class_ptr &&obj_ptr, Args &&... args):
        obj_ptr(std::forward<Class_ptr>(obj_ptr)), method(method),
        args(std::forward<Args>(args)...)
    {}

    /** Move constructor. */
    Callback(Callback &&) = default;

    /** Execute callback. */
    virtual typename Base_type::Result_t
    operator ()() override
    {
        return Invoke(typename callback_internal::Sequence_generator
                        <sizeof...(Args)>::Sequence_type());
    }

    /** Get reference to an argument at the specified position.
     * Template parameters:
     * - *arg_idx* Index of the argument to get.
     * @return Reference to the stored argument.
     */
    template <size_t arg_idx>
    Arg_type<arg_idx> &
    Get_arg()
    {
        return std::get<arg_idx>(args);
    }

private:
    /** Object instance to call method in. */
    typename std::remove_reference<Class_ptr>::type obj_ptr;
    /** Method pointer to call in the provided object instance. */
    typename callback_internal::Adapt_callable_type<Method>::type method;
    /** User provided arguments pack. */
    Args_tuple args;

    /** Invoke callable entity with user provided arguments. */
    template <int... s>
    typename Base_type::Result_t
    Invoke(callback_internal::Sequence<s...>)
    {
        return ((*obj_ptr).*method)(std::get<s>(args)...);
    }
};

/** Create a callback.
 *
 * @param callable Any callable object type (function, lambda, class with
 *      overloaded call operator etc.). It may be a class member function
 *      pointer. In this case the first argument should be object instance
 *      pointer (may be any object which can be used with pointer semantic).
 * @param args Arguments pack.
 * @return Callback specified with user arguments.
 * @see Callback
 */
template <class Callable, typename... Args>
typename Callback<Callable, void, Args...>::Ptr
Make_callback(Callable &&callable, Args&& ...args)
{
    return Callback<Callable,
                    void,
                    Args...>::Create
        (std::forward<Callable>(callable), std::forward<Args>(args)...);
}

namespace callback_internal {

/** Helper class for creating dummy callbacks. */
template <class Result, typename... Args>
class Dummy_callback_helper {
public:
    /** Callable type. */
    typedef std::function<Result(Args...)> Callable;
    /** Resulted callback type. */
    typedef Callback<Callable, void, Args...> Callback_type;

    /** Create callable object. */
    static Callable
    Create()
    {
        return [](Args __UNUSED... args) { return Result(); };
    }
};

/** Specialization for void return type. */
template <typename... Args>
class Dummy_callback_helper<void, Args...> {
public:
    /** Callable type. */
    typedef std::function<void(Args...)> Callable;
    /** Resulted callback type. */
    typedef Callback<Callable, void, Args...> Callback_type;

    /** Create callable object. */
    static Callable
    Create()
    {
        return [](Args __UNUSED... args) {};
    }
};

} /* namespace callback_internal */

/** Create dummy callback. Such callback can be used as placeholder, for example,
 * in default value for handler argument. It uses default constructor for
 * mandatory arguments and return value. It does nothing when fired.
 * Template parameters:
 * - *Result* Type of return value.
 * - *Args* Types for mandatory arguments if any.
 */
template <class Result, typename... Args>
typename callback_internal::Dummy_callback_helper<Result, Args...>::Callback_type::Ptr
Make_dummy_callback()
{
    typedef callback_internal::Dummy_callback_helper<Result, Args...> Helper;
    return Make_callback<typename Helper::Callable>(Helper::Create(), Args()...);
}

namespace callback_internal {

/** Helper structure for checking callback argument types. */
template <class Callback, size_t idx, typename... Args>
struct Callback_args_checker;

/** No arguments to check case. */
template <class Callback>
struct Callback_args_checker<Callback, 0> {
    /** Always passes the check. */
    constexpr static bool value = true;
};

/** Tail recursion case. */
template <class Callback, size_t idx, typename Arg>
struct Callback_args_checker<Callback, idx, Arg> {
    /** Corresponding real argument type in the callback. */
    typedef typename Callback::template Arg_type<idx> Cbk_arg;
    /** Indicates whether the check passed or failed. */
    constexpr static bool value = std::is_convertible<Cbk_arg, Arg>::value;

    static_assert(value, "Argument type mismatch");
};

/** Recursion unrolling template. */
template <class Callback, size_t idx, typename Arg, typename... Args>
struct Callback_args_checker<Callback, idx, Arg, Args...> {
    /** Corresponding real argument type in the callback. */
    typedef typename Callback::template Arg_type<idx> Cbk_arg;
    /** Indicates whether the check passed or failed. */
    constexpr static bool value = std::is_convertible<Cbk_arg, Arg>::value &&
        Callback_args_checker<Callback, idx + 1, Args...>::value;

    static_assert(value, "Argument type mismatch");
};

} /* namespace callback_internal */

/** Check if the specified callback type corresponds to the partial prototype.
 * All types are checked for implicit conversion possibility from real type to
 * the expected one.
 *
 * Template parameters:
 * - *Callback_ptr* Type of callback pointer.
 * - *Result* Expected result type.
 * - *Args* Expected first arguments type.
 */
template <class Callback_ptr, typename Result, typename... Args>
constexpr void
Callback_check_type()
{
    typedef typename
        std::remove_reference<decltype(*std::declval<Callback_ptr>())>::type Callback;

    /* Check result type. */
    static_assert(std::is_convertible<typename Callback::Base_type::Result_t, Result>::value,
                  "Callback result value type mismatch");
    /* Check argument types. */
    static_assert(callback_internal::Callback_args_checker<Callback, 0, Args...>::value,
                  "Arguments type mismatch");
}


#ifndef NO_DOXYGEN
namespace callback_internal {

/** Helper class for using by Callback_forced_args.
 * @param Callable User provided callable object.
 * @param Enable Dummy type for template conditional specialization.
 * @param Forced_args_tuple Tuple type with forced arguments types.
 * @param Args User arguments.
 */
template <class Callable, typename Enable, typename... Args>
class Callback_forced_args_helper {
public:
    template <typename... Forced_args>
    static typename Callback<Callable, void, Forced_args..., Args...>::Ptr
    Create(Callable &&callable, std::tuple<Forced_args...> &&forced_args,
           Args &&... args)
    {
        return Create_impl<decltype(forced_args)>(
            std::forward<Callable>(callable),
            std::tuple_cat(std::forward<std::tuple<Forced_args...>>(forced_args),
                           std::forward_as_tuple(std::forward<Args>(args)...)),
            typename callback_internal::Sequence_generator<sizeof...(Forced_args) + sizeof...(Args)>::Sequence_type());
    }

    /** Resulted callback class type. */
    template <class Forced_args_tuple>
    using Callback_type = decltype(Callback_forced_args_helper::Create(
                                          std::declval<Callable>(),
                                          std::declval<Forced_args_tuple>(),
                                          std::declval<Args>()...));
private:
    template <class Forced_args_tuple, class All_args_tuple, int... args_seq>
    static typename Callback_type<Forced_args_tuple>::Ptr
    Create_impl(Callable &&callable, All_args_tuple &&all_args_tuple,
                callback_internal::Sequence<args_seq...>)
    {
        return Make_callback(
            std::forward<Callable>(callable),
            std::forward<typename std::tuple_element<args_seq, All_args_tuple>::type>
                (std::get<args_seq>(all_args_tuple))...);
    }
};

/** Specialization for member function case. */
template <class Method, class Class_ptr, typename... Args>
class Callback_forced_args_helper<
    Method,
    typename std::enable_if<callback_internal::Is_method_ptr<Method>()>::type,
                            Class_ptr, Args...> {
public:
    template <typename... Forced_args>
    static typename Callback<Method, void, Class_ptr, Forced_args..., Args...>::Ptr
    Create(Method &&method, std::tuple<Forced_args...> &&forced_args,
           Class_ptr &&class_ptr, Args &&... args)
    {
        return Create_impl<decltype(forced_args)>(
            std::forward<Method>(method),
            std::tuple_cat(std::forward_as_tuple(std::forward<Class_ptr>(class_ptr)),
                           std::forward<std::tuple<Forced_args...>>(forced_args),
                           std::forward_as_tuple(std::forward<Args>(args)...)),
            typename callback_internal::Sequence_generator<1 + sizeof...(Forced_args) + sizeof...(Args)>::Sequence_type());
    }

    /** Resulted callback class type. */
    template <class Forced_args_tuple>
    using Callback_type = decltype(Callback_forced_args_helper::Create(
                                          std::declval<Method>(),
                                          std::declval<Forced_args_tuple>(),
                                          std::declval<Class_ptr>(),
                                          std::declval<Args>()...));
private:
    template <class Forced_args_tuple, class All_args_tuple, int... args_seq>
    static typename Callback_type<Forced_args_tuple>::Ptr
    Create_impl(Method &&method, All_args_tuple &&all_args_tuple,
                callback_internal::Sequence<args_seq...>)
    {
        return Make_callback(
            std::forward<Method>(method),
            std::forward<typename std::tuple_element<args_seq, All_args_tuple>::type>
                (std::get<args_seq>(all_args_tuple))...);
    }
};

} /* namespace callback_internal */
#endif


/** Helper class for defining custom callback creation functions which force
 * several first arguments for the user callback.
 * @param Callable User provided callable object.
 * @param Forced_args_tuple std::tuple for enforced arguments.
 * @param Args User arguments for the callable. If the callable object is
 *      member function then the first user argument should be pointer (any
 *      smart pointer accepted) to the object instance.
 *
 * Usage example:
 * {@code
 *
 * // Enforce "int" argument with value "10".
 * template <class Callable, typename... Args>
 * typename Callback_forced_args<Callable, std::tuple<int>, Args...>::Callback_type::Ptr
 * Make_my_callback(Callable &&callable, Args &&... args)
 * {
 *    return Callback_forced_args<Callable, std::tuple<int>, Args...>::Create(
 *        std::forward<Callable>(callable), std::forward_as_tuple<int>(10),
 *        std::forward<Args>(args)...);
 * }
 *
 * }
 */
template <class Callable, class Forced_args_tuple, typename... Args>
class Callback_forced_args:
    public callback_internal::Callback_forced_args_helper<Callable, void, Args...>::
           template Callback_type<Forced_args_tuple> {

public:
    /** Helper type. */
    typedef callback_internal::Callback_forced_args_helper<Callable, void, Args...> Helper;
    /** Resulted callback type. */
    typedef typename Helper::template Callback_type<Forced_args_tuple> Callback_type;

    /** Create callback with forced first arguments.
     *
     * @param callable User provided callable object. Should be forwarded by
     *      std::forward().
     * @param forced_args_tuple Tuple with first arguments values. Should be
     *      created by std::forward_as_tuple().
     * @param args User provided arguments. Should be forwarded by std::forward().
     * @return Pointer to created callback.
     */
    static typename Callback_type::Ptr
    Create(Callable &&callable, Forced_args_tuple &&forced_args_tuple,
           Args &&... args)
    {
        return Helper::Create(std::forward<Callable>(callable),
                              std::forward<Forced_args_tuple>(forced_args_tuple),
                              std::forward<Args>(args)...);
    }
};

namespace internal {

/** Assign_ptr_tuple implementation specialization for
 * tail last recursive call.
 */
template <class Ptr_tuple, size_t arg_idx>
void
Assign_ptr_tuple_impl(Ptr_tuple &tuple __UNUSED)
{}

/** Assign_ptr_tuple implementation recursive definition. */
template <class Ptr_tuple, size_t arg_idx, typename Arg, typename... Args>
void
Assign_ptr_tuple_impl(Ptr_tuple &tuple, Arg &&arg, Args &&... args)
{
    *std::get<arg_idx>(tuple) = std::forward<Arg>(arg);
    Assign_ptr_tuple_impl<Ptr_tuple, arg_idx + 1, Args...>
        (tuple, std::forward<Args>(args)...);
}

/** Helper function for assigning values to location pointers by pointers
 * stored in a tuple. */
template <class Ptr_tuple, typename... Args>
void
Assign_ptr_tuple(Ptr_tuple &tuple, Args &&... args)
{
    Assign_ptr_tuple_impl<Ptr_tuple, 0, Args...>(tuple, std::forward<Args>(args)...);
}

} /* namespace internal */

/** Helper class for proxying callback invocation. It is aware of specified
 * number of first arguments to the target callback.
 *
 * @param Result Type of the callback invocation result.
 * @param Args Type of the first arguments.
 */
template <class Result, typename... Args>
class Callback_proxy {
public:
    /** Base type of the underlying callback object. */
    typedef Callback_base<Result> Callback_type;
    /** Pointer to the underlying callback object. */
    typedef typename Callback_type::template Ptr<> Callback_ptr;

private:
    /** Type for tuple which can store references to the first arguments. */
    typedef std::tuple<typename std::add_lvalue_reference<Args>::type...> Args_ref_tuple;
    /** Type for tuple which can store pointers to the first arguments. */
    typedef std::tuple<typename std::add_pointer<Args>::type...> Args_ptr_tuple;

public:
    /** Creates reference type to the argument with index @a arg_idx. */
    template <size_t arg_idx>
    using Arg_ref_type = typename std::tuple_element<arg_idx, Args_ref_tuple>::type;

    /** Creates pointer type to the argument with index @a arg_idx. */
    template <size_t arg_idx>
    using Arg_ptr_type = typename std::tuple_element<arg_idx, Args_ptr_tuple>::type;

    /** Default constructor creates unbound instance. */
    Callback_proxy() = default;

    /** Constructs instance bound to the provided callback object. */
    template <class Callback_ptr>
    Callback_proxy(Callback_ptr cbk):
        cbk(cbk), args_ptr(Get_args_ptr_tuple(cbk, Args_seq()))
    {
        Callback_check_type<Callback_ptr, Result, Args...>();
    }

    /** Set arguments values for callback invocation. */
    template <typename... Invoke_args>
    void
    Set_args(Invoke_args &&... invoke_args) const
    {
        internal::Assign_ptr_tuple(args_ptr, std::forward<Invoke_args>(invoke_args)...);
    }

    /** Invoke the callback with the specified first arguments. */
    template <typename... Invoke_args>
    Result
    operator()(Invoke_args &&... invoke_args) const
    {
        Set_args<Invoke_args...>(std::forward<Invoke_args>(invoke_args)...);
        return Invoke();
    }

    /** Invoke the callback with the previously set argument values. */
    Result
    Invoke() const
    {
        return cbk();
    }

    /** Get the associated callback. */
    Callback_ptr
    Get_callback()
    {
        return cbk;
    }

    /** Check if the proxy object is bound to some callback object. */
    explicit operator bool() const
    {
        return cbk != nullptr;
    }

    /** Support implicit cast to callback base class. */
    operator Callback_ptr()
    {
        return cbk;
    }

    /** Get reference to an argument at the specified position.
     * Template parameters:
     * - *arg_idx* Index of the argument to get.
     * @return Reference to the stored argument.
     */
    template <size_t arg_idx>
    Arg_ref_type<arg_idx>
    Get_arg()
    {
        /*
         * Proxy should be bound to be able to access arguments.
         * In release build the code will anyway crash somewhere else.
         */
        ASSERT(*this);
        return *std::get<arg_idx>(args_ptr);
    }

    /** Set argument value at the specified position.
     * Template parameters:
     * - *arg_idx* Argument position.
     * @param arg Argument value.
     */
    template <size_t arg_idx, typename Arg_type>
    void
    Set_arg(Arg_type &&arg)
    {
        Get_arg<arg_idx>() = std::forward<Arg_type>(arg);
    }

    /** Equality operator. Proxies pointing to the same callback are
     * considered the same.
     */
    bool
    operator ==(const Callback_proxy& other) const
    {
        return other.cbk == cbk;
    }

    /** Non-equality operator. Proxies pointing to the different callbacks
     * are considered different.
     */
    bool
    operator !=(const Callback_proxy& other) const
    {
        return other.cbk != cbk;
    }

    /** Callback proxy hasher based on referenced callback. */
    class Hasher
    {
    public:
        /** Hash value operator. */
        size_t
        operator ()(const Callback_proxy& proxy) const
        {
            static std::hash<decltype(proxy.cbk.get())> hasher;

            return hasher(proxy.cbk.get());
        }
    };

private:
    /** Arguments sequence type. */
    typedef typename callback_internal::Sequence_generator<sizeof...(Args)>::Sequence_type Args_seq;

    /** Pointer to target callback. */
    mutable Callback_ptr cbk;
    /** Tuple with pointers to the first arguments. */
    Args_ptr_tuple args_ptr;

    /** Get tuple with pointers to the specified number of the first arguments
     * for the specified callback.
     */
    template <class Callback_ptr, int... args_seq>
    static Args_ptr_tuple
    Get_args_ptr_tuple(Callback_ptr cbk, callback_internal::Sequence<args_seq...>)
    {
        return Args_ptr_tuple(&cbk->template Get_arg<args_seq>()...);
    }
};

} /* namespace vsm */

#endif /* _CALLBACK_H_ */
