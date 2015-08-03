// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Platform-dependent definitions.
 */

#ifndef __unix__
#ifndef __APPLE__
#error "This header should be included only in unix build."
#endif
#endif

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <ugcs/vsm/endian.h>


/** Symbol used as file-system path separator on a target platform. */
#define PATH_SEPARATOR      '/'
/** Characters sequence used as line terminator in text streams. */
#define LINE_TERMINATOR     "\n"

#endif /* PLATFORM_H_ */
