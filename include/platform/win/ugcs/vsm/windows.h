// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * This is to ease windows integration of SDK.
 */

#ifndef _WIN32
#error "This header should be included only in Windows build."
#endif

#ifndef UGCS_WINDOWS_H_
#define UGCS_WINDOWS_H_

#include <windows.h>
/* Some weird code can define these macros in Windows headers. */
#ifdef ERROR
#undef ERROR
#endif
#ifdef interface
#undef interface
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

#endif /* UGCS_WINDOWS_H_ */
