// Copyright (c) 2014, Smart Projects Holdings Ltd
// All rights reserved.
// See LICENSE file for license details.

/*
 * shared_mutex_file.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: janis
 */

#include <vsm/shared_mutex_file.h>

namespace vsm
{

Shared_mutex_file::Shared_mutex_file(const std::string& name, File_processor::Ptr fp)
{
    char dest[200];
    ExpandEnvironmentStrings("%SystemRoot%\\Temp\\vsm_shared_mutex_", dest, 200);
    stream = fp->Open(dest + name, "rx");
}

} /* namespace vsm */
