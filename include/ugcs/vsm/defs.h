// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file defs.h
 * Common preprocessor definitions.
 */

#ifndef _UGCS_VSM_DEFS_H_
#define _UGCS_VSM_DEFS_H_

/** Specify that a function has format arguments (like printf or scanf). See
 * 'format' attribute description in GCC documentation (XXX).
 */
// XXX ifdef GCC
#ifdef __unix__
#define __FORMAT(type, fmt_idx, arg_idx)    \
    __attribute__((format(type, fmt_idx, arg_idx)))
#else /* __unix__ */
/* Windows has improper size of long type which causes undesired warning.
 * Disable format validation there.
 */
#define __FORMAT(type, fmt_idx, arg_idx)
#endif /* __unix__ */

/** Use with unused arguments if you like to declare that it is not (yet) used
 * a the function.
 */
#define __UNUSED    __attribute__((unused))

/** Pack structure or class, i.e. do not allow the compiler to insert paddings
 * for members alignment.
 */
#define __PACKED    __attribute__((packed))

// Use this to define function to get VSM name. It is used by SDK when registering VSM with server.
// It must be defined in VSM sources (typically in main.cpp)
// Otherwise there will be an undefined reference linker error against Get_vsm_name().

// DEFINE_DEFAULT_VSM_NAME returns the CMAKE_PROJECT_NAME defined by SDK buildsystem.
#define DEFINE_DEFAULT_VSM_NAME namespace ugcs{namespace vsm{const char* Get_vsm_name() {return VSM_PROJECT_NAME;}}}

// DEFINE_VSM_NAME can be used instead to override the default VSM name
#define DEFINE_VSM_NAME(x) namespace ugcs{namespace vsm{const char* Get_vsm_name() {return x;}}}

#endif /* _UGCS_VSM_DEFS_H_ */
