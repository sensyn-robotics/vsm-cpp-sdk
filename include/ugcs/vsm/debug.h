// Copyright (c) 2018, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/**
 * @file debug.h
 *
 * Debugging and troubleshooting helpers.
 */

#ifndef _UGCS_VSM_DEBUG_H_
#define _UGCS_VSM_DEBUG_H_

#include <ugcs/vsm/log.h>

#include <stdexcept>

/** Assertion action which is done when assertion fires.
 * @param cond_str Stringified condition expression.
 */
#ifdef UNITTEST

#define ASSERT_IMPL(cond_str) do { \
    VSM_EXCEPTION(::ugcs::vsm::Debug_assert_exception, cond_str); \
} while (false)

#else /* UNITTEST */

#define ASSERT_IMPL(cond_str) do { \
    /* More convenient than exception since the back-trace will point to the \
     * assert in the generated core dump. \
     */ \
    abort(); \
} while (false)

#endif /* UNITTEST */

#ifdef DEBUG

/** Verify that expression is true in debug build. In release build the
 * expression is not evaluated.
 * @param x Expression to check. Assertion fires when false.
 */
#define ASSERT(x) do { \
    if (!(x)) { \
        LOG_ERROR("Assert failed: '%s'", # x); \
        if (std::uncaught_exception()) { \
            LOG_ERROR("WARNING: uncaught exception active when assertion fired!"); \
        } \
        ASSERT_IMPL(# x); \
    } \
} while (false)

/** Verify that expression is equal to the expected value in debug build. In
 * release build the expression is evaluated but not verified.
 * @param x Expression to check. Assertion fires when false.
 */
#define VERIFY(x, expected) do { \
    if ((x) != (expected)) { \
        LOG_ERROR("Verification failed: '%s'", # x " == " # expected); \
        ASSERT_IMPL(# x " == " # expected); \
    } \
} while (false)

#else /* DEBUG */

/** No action in release. */
#define ASSERT(x)

/** Evaluate in release, but don't compare. */
#define VERIFY(x, expected)       x

#endif /* DEBUG */

#endif /* _UGCS_VSM_DEBUG_H_ */
