// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

#include <ugcs/vsm/log.h>
#include <ugcs/vsm/java.h>

#include <thread>

using namespace ugcs::vsm;

JavaVM *Java::java_vm;
jobject Java::java_vsm_obj;
thread_local JNIEnv *java_jni_env = nullptr;

void
Java::Initialize(JNIEnv *env, jobject vsm_obj)
{
    java_vsm_obj = env->NewGlobalRef(vsm_obj);
    if (env->GetJavaVM(&java_vm) != JNI_OK) {
        LOG_ERROR("Failed to get Java VM reference");
    }
}

void
Java::Detach_current_thread()
{
    if (java_jni_env) {
        java_vm->DetachCurrentThread();
        java_jni_env = nullptr;
        LOG("Detached Java JNI environment from thread %zu",
            std::hash<std::thread::id>()(std::this_thread::get_id()));
    }
}

Java::Env
Java::Get_env(JNIEnv *env)
{
    if (!env) {
        if (!java_jni_env) {
            if (java_vm->AttachCurrentThread(&java_jni_env, nullptr) != JNI_OK) {
                VSM_EXCEPTION(Internal_error_exception, "Failed to attach thread to JVM");
            }
            LOG("Attached Java JNI environment to thread %zu",
                std::hash<std::thread::id>()(std::this_thread::get_id()));
        }
        env = java_jni_env;
    }
    return Env(env);
}
