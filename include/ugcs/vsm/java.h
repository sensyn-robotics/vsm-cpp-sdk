// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#ifndef JAVA_H_
#define JAVA_H_

#include <jni.h>

namespace ugcs {
namespace vsm {

namespace java_internals {

#define JAVA_EXCEPTION_CHECK(__env) do { \
    if (__env->ExceptionCheck()) { \
        VSM_EXCEPTION(ugcs::vsm::Internal_error_exception, "Java exception occurred"); \
    } \
} while (false)

template <typename T_ret>
struct MethodCallSelector {};

template <>
struct MethodCallSelector<void> {
    template <typename... T_args>
    static void
    Call(JNIEnv *env, jobject obj, jmethodID method_id, T_args... args)
    {
        env->CallVoidMethod(obj, method_id, args...);
        JAVA_EXCEPTION_CHECK(env);
    }
};

#define __JAVA_CONCAT_(x, y) x ## y
#define __JAVA_CONCAT(x, y) __JAVA_CONCAT_(x, y)

#define JAVA_DEF_METHOD_SELECTOR(__type, __name) \
    template <> \
    struct MethodCallSelector<__type> { \
        template <typename... T_args> \
        static __type \
        Call(JNIEnv *env, jobject obj, jmethodID method_id, T_args... args) \
        { \
            __type res = __JAVA_CONCAT( \
                __JAVA_CONCAT(env->Call,__name), Method)(obj, method_id, args...); \
            JAVA_EXCEPTION_CHECK(env); \
            return res; \
        } \
    };

JAVA_DEF_METHOD_SELECTOR(jobject, Object)
JAVA_DEF_METHOD_SELECTOR(jboolean, Boolean)
JAVA_DEF_METHOD_SELECTOR(jbyte, Byte)
JAVA_DEF_METHOD_SELECTOR(jchar, Char)
JAVA_DEF_METHOD_SELECTOR(jshort, Short)
JAVA_DEF_METHOD_SELECTOR(jint, Int)
JAVA_DEF_METHOD_SELECTOR(jlong, Long)
JAVA_DEF_METHOD_SELECTOR(jfloat, Float)
JAVA_DEF_METHOD_SELECTOR(jdouble, Double)

class ArrayBase {
public:
    ArrayBase(JNIEnv *env, jobject array):
        env(env), array(reinterpret_cast<jarray>(array))
    {}

    void
    Release()
    {
        env->DeleteLocalRef(array);
    }
protected:
    JNIEnv *env;
    jarray array;
};

template <typename T>
class PrimitiveArray: public ArrayBase {
public:
    using ArrayBase::ArrayBase;

    //XXX
};

class ObjectArray: public ArrayBase {
public:
    using ArrayBase::ArrayBase;

    size_t
    Size() const
    {
        return env->GetArrayLength(array);
    }

    jobject
    Get(size_t idx)
    {
        jobject res = env->GetObjectArrayElement(
            reinterpret_cast<jobjectArray>(array), idx);
        JAVA_EXCEPTION_CHECK(env);
        return res;
    }

    jobject
    operator[](size_t idx)
    {
        return Get(idx);
    }
};

template <typename T>
class Array: PrimitiveArray<T> {
    using PrimitiveArray<T>::PrimitiveArray;
};

template <>
class Array<jobject>: public ObjectArray {
    using ObjectArray::ObjectArray;
};

} /* namespace java_internals */

class Java {
public:

    template <typename T>
    using Array = java_internals::Array<T>;

    class Env {
    public:
        Env(JNIEnv *env):
            env(env)
        {}

        JNIEnv *
        operator ->()
        {
            return env;
        }

        template <typename T_ret, typename... T_args>
        T_ret
        CallMethod(jobject obj, const std::string &method_name,
                   const std::string &method_signature, T_args... args)
        {
            jclass cls = env->GetObjectClass(obj);
            JAVA_EXCEPTION_CHECK(env);
            jmethodID mid = env->GetMethodID(cls, method_name.c_str(),
                                             method_signature.c_str());
            JAVA_EXCEPTION_CHECK(env);
            env->DeleteLocalRef(cls);
            return java_internals::MethodCallSelector<T_ret>::Call
                (env, obj, mid, args...);
        }

        template <typename T_ret, typename... T_args>
        T_ret
        CallVsmMethod(const std::string &method_name,
                      const std::string &method_signature, T_args... args)
        {
            return CallMethod<T_ret, T_args...>(java_vsm_obj, method_name,
                                                method_signature, args...);
        }

        template <typename T>
        Array<T>
        GetArray(jobject array)
        {
            return Array<T>(env, array);
        }

        std::string
        GetString(jobject s)
        {
            jboolean isCopy;
            const char *chars = env->GetStringUTFChars(reinterpret_cast<jstring>(s),
                                                       &isCopy);
            if (!chars) {
                VSM_EXCEPTION(ugcs::vsm::Internal_error_exception,
                              "Failed to get Java string data");
            }
            std::string res(chars);
            env->ReleaseStringUTFChars(reinterpret_cast<jstring>(s), chars);
            return res;
        }

        jstring
        WrapString(const std::string &s)
        {
            return env->NewStringUTF(s.c_str());
        }

    private:
        JNIEnv *env;
    };

    static void
    Initialize(JNIEnv *env, jobject vsm_obj);

    /** Must be called before attached to VM thread is exiting. */
    static void
    Detach_current_thread();

    static Env
    Get_env(JNIEnv *env = nullptr);

private:
    /** Java VM instance. */
    static JavaVM *java_vm;
    /** VSM instance on Java side. */
    static jobject java_vsm_obj;
};

} /* namespace vsm */
} /* namespace ugcs */

#endif /* JAVA_H_ */
