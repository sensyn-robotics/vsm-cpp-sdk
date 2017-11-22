// Copyright (c) 2017, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file defs.h
 * Common preprocessor definitions.
 */

#ifndef DEFS_H_
#define DEFS_H_

/** Specify that a function has format arguments (like printf or scanf). See
 * 'format' attribute description in GCC documentation (XXX).
 */
//XXX ifdef GCC
#ifdef __unix__
#define __FORMAT(type, fmt_idx, arg_idx)    \
    __attribute__ ((format(type, fmt_idx, arg_idx)))
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

#endif /* DEFS_H_ */
