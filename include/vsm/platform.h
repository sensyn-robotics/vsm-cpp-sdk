// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * Platform-dependent definitions.
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <vsm/endian.h>

namespace vsm {

#ifdef _WIN32

#define PATH_SEPARATOR      '\\'
#define LINE_TERMINATOR     "\r\n"

#elif defined(__unix__)

/** Symbol used as file-system path separator on a target platform. */
#define PATH_SEPARATOR      '/'
/** Characters sequence used as line terminator in text streams. */
#define LINE_TERMINATOR     "\n"

#endif

} /* namespace vsm */

#endif /* PLATFORM_H_ */
